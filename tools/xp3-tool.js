// SPDX-License-Identifier: AGPL-3.0-only
// XP3 format: src/core/base/XP3Archive.cpp. File slices and streaming ZIP64.
const XP3Tool = (() => {
  const extensions = new Set(['tjs', 'ks', 'scn', 'ini', 'cfg', 'txt', 'xml', 'json', 'csv', 'asd', 'tpm', 'dll']);
  const view = b => new DataView(b.buffer, b.byteOffset, b.byteLength);
  const u16 = (b, p = 0) => view(b).getUint16(p, true);
  const u32 = (b, p = 0) => view(b).getUint32(p, true);
  const u64 = (b, p = 0) => {
    const value = Number(view(b).getBigUint64(p, true));
    if (!Number.isSafeInteger(value)) throw Error('长度超出整数精度范围');
    return value;
  };
  async function read(file, start, size) {
    if (!Number.isSafeInteger(start) || !Number.isSafeInteger(size) || start < 0 || size < 0 || size > file.size - start)
      throw Error('XP3 长度/位置异常');
    return new Uint8Array(await file.slice(start, start + size).arrayBuffer());
  }
  function concat(parts) {
    const out = new Uint8Array(parts.reduce((sum, b) => sum + b.length, 0));
    let position = 0;
    for (const b of parts) { out.set(b, position); position += b.length; }
    return out;
  }
  function* chunks(bytes) {
    let pos = 0;
    while (pos < bytes.length) {
      if (pos + 12 > bytes.length) throw Error('索引块头损坏');
      const kind = String.fromCharCode(...bytes.subarray(pos, pos + 4)), size = u64(bytes, pos + 4);
      pos += 12;
      if (size > bytes.length - pos) throw Error('索引块长度损坏');
      yield [kind, bytes.subarray(pos, pos + size)];
      pos += size;
    }
  }
  function decompressed(stream) {
    if (typeof DecompressionStream === 'undefined') throw Error('此浏览器不支持 zlib 解压，请使用 Python 工具');
    return stream.pipeThrough(new DecompressionStream('deflate'));
  }
  async function inflate(bytes, size) {
    const reader = decompressed(new Blob([bytes]).stream()).getReader(), parts = [];
    let total = 0;
    try {
      while (true) {
        const {done, value} = await reader.read();
        if (done) break;
        total += value.length;
        if (total > size) throw Error('解压长度超出索引声明');
        parts.push(value);
      }
    } finally { await reader.cancel(); }
    if (total !== size) throw Error('压缩索引长度不正确');
    return concat(parts);
  }
  async function index(file) {
    if (String([...await read(file, 0, 11)]) !== '88,80,51,13,10,32,10,26,139,103,1')
      throw Error('不是标准 XP3，或使用专用索引格式');
    let position = u64(await read(file, 11, 8));
    const seen = new Set(), entries = [];
    while (true) {
      if (seen.has(position)) throw Error('索引形成循环');
      seen.add(position);
      const header = await read(file, position, 9), flags = header[0], stored = u64(header, 1);
      position += 9;
      let data;
      if ((flags & 7) === 1) {
        const size = u64(await read(file, position, 8)); position += 8;
        data = await inflate(await read(file, position, stored), size);
      } else if ((flags & 7) === 0) data = await read(file, position, stored);
      else throw Error('未知索引压缩方法');
      position += stored;
      const next = flags & 128 ? u64(await read(file, position, 8)) : null;
      for (const [kind, block] of chunks(data)) {
        if (kind !== 'File') continue;
        const fields = Object.fromEntries(chunks(block)), info = fields.info, segm = fields.segm;
        if (!info || info.length < 22 || !segm || segm.length % 28 || !fields.adlr || fields.adlr.length !== 4)
          throw Error('文件索引损坏');
        const length = u16(info, 20);
        if (22 + length * 2 > info.length) throw Error('文件名长度损坏');
        const name = new TextDecoder('utf-16le', {fatal: true}).decode(info.subarray(22, 22 + length * 2)), pieces = [];
        for (let pos = 0; pos < segm.length; pos += 28) {
          const method = u32(segm, pos) & 7, offset = u64(segm, pos + 4), size = u64(segm, pos + 12), stored = u64(segm, pos + 20);
          if (method > 1 || stored > file.size - offset || (method === 0 && size !== stored)) throw Error('分段格式损坏');
          pieces.push({method, offset, size, stored});
        }
        if (pieces.reduce((s, p) => s + p.size, 0) !== u64(info, 4) || pieces.reduce((s, p) => s + p.stored, 0) !== u64(info, 12))
          throw Error('分段长度不一致');
        entries.push({name, flags: u32(info), size: u64(info, 4), checksum: u32(fields.adlr), pieces});
      }
      if (next === null) break;
      position = next;
    }
    return entries;
  }
  function safeName(name) {
    const normalized = name.replaceAll('\\', '/'), parts = normalized.split('/');
    if (!name || name.includes('\0') || normalized.startsWith('/') || name.includes(':') || parts.includes('..'))
      throw Error('不安全的归档路径');
    const result = parts.filter(p => p && p !== '.').join('/');
    if (!result) throw Error('空归档路径');
    return result;
  }
  async function* contents(file, entry) {
    for (const piece of entry.pieces) {
      let stream = file.slice(piece.offset, piece.offset + piece.stored).stream();
      if (piece.method === 1) stream = decompressed(stream);
      const reader = stream.getReader();
      let written = 0;
      try {
        while (true) {
          const {done, value} = await reader.read();
          if (done) break;
          written += value.length;
          if (written > piece.size) throw Error('解压后的分段超过声明大小');
          yield value;
        }
      } finally { await reader.cancel(); }
      if (written !== piece.size) throw Error('分段解压长度不一致');
    }
  }
  const crcTable = Uint32Array.from({length: 256}, (_, i) => {
    let c = i;
    for (let bit = 0; bit < 8; ++bit) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    return c >>> 0;
  });
  const crcUpdate = (crc, data) => { for (const b of data) crc = crcTable[(crc ^ b) & 255] ^ (crc >>> 8); return crc; };
  async function inspect(file, entry) {
    let a = 1, b = 0, crc = 0xffffffff, prefix = [], bytes = 0;
    for await (const data of contents(file, entry)) {
      crc = crcUpdate(crc, data);
      for (let pos = 0; pos < data.length; pos += 5552) {
        const end = Math.min(data.length, pos + 5552);
        for (let i = pos; i < end; ++i) { a += data[i]; b += a; }
        a %= 65521; b %= 65521;
      }
      if (prefix.length < 32) prefix.push(...data.subarray(0, 32 - prefix.length));
      bytes += data.length;
    }
    return {checksum: ((b << 16) | a) >>> 0, crc: (crc ^ 0xffffffff) >>> 0, bytes,
      prefix_hex: prefix.map(b => b.toString(16).padStart(2, '0')).join('')};
  }
  function header(size, fields) {
    const bytes = new Uint8Array(size), v = view(bytes);
    for (const [offset, width, value] of fields) {
      if (width === 2) v.setUint16(offset, value, true);
      else if (width === 8) v.setBigUint64(offset, BigInt(value), true);
      else v.setUint32(offset, value, true);
    }
    return bytes;
  }
  class Zip64Writer {
    constructor(sink) { this.sink = sink; this.offset = 0n; this.central = []; }
    async write(bytes) { await this.sink.write(bytes); this.offset += BigInt(bytes.length); }
    async add(name, size, crc, stream) {
      const encoded = new TextEncoder().encode(name), start = this.offset;
      if (encoded.length > 65535) throw Error('ZIP 文件名过长');
      // Streaming, stored entries with ZIP64 local extras and data descriptors.
      await this.write(header(30, [[0,4,0x04034b50],[4,2,45],[6,2,0x808],[18,4,0xffffffff],[22,4,0xffffffff],[26,2,encoded.length],[28,2,20]]));
      await this.write(encoded);
      await this.write(header(20, [[0,2,1],[2,2,16],[4,8,0],[12,8,0]]));
      let actual = 0;
      for await (const bytes of stream) { actual += bytes.length; await this.write(bytes); }
      if (actual !== size) throw Error('ZIP 输出长度不一致');
      await this.write(header(24, [[0,4,0x08074b50],[4,4,crc],[8,8,size],[16,8,size]]));
      this.central.push(header(46, [[0,4,0x02014b50],[4,2,45],[6,2,45],[8,2,0x808],[16,4,crc],[20,4,0xffffffff],[24,4,0xffffffff],[28,2,encoded.length],[30,2,28],[42,4,0xffffffff]]), encoded,
        header(28, [[0,2,1],[2,2,24],[4,8,size],[12,8,size],[20,8,start]]));
    }
    async close() {
      const centralStart = this.offset, count = this.central.length / 3;
      for (const bytes of this.central) await this.write(bytes);
      const centralSize = this.offset - centralStart, zip64Start = this.offset;
      await this.write(header(56, [[0,4,0x06064b50],[4,8,44],[12,2,45],[14,2,45],[24,8,count],[32,8,count],[40,8,centralSize],[48,8,centralStart]]));
      await this.write(header(20, [[0,4,0x07064b50],[8,8,zip64Start],[16,4,1]]));
      await this.write(header(22, [[0,4,0x06054b50],[8,2,Math.min(count,65535)],[10,2,Math.min(count,65535)],[12,4,0xffffffff],[16,4,0xffffffff]]));
      return this.sink.close();
    }
  }
  async function exportToSink(file, sink, scripts = true, progress = () => {}, retainUnverified = true) {
    const entries = await index(file), names = new Set(), writer = new Zip64Writer(sink);
    const report = {archive: file.name, mode: scripts ? 'scripts' : 'all', entries: entries.length,
      exported: [], unverified: [], skipped: [], filtered: 0,
      notes: 'files/ 为校验一致的内容；unverified/ 为校验不一致的诊断字节，可能是自定义哈希、加密或损坏，不能据此断言密文。不实现游戏专用解密。'};
    for (const entry of entries) {
      if (scripts && !extensions.has(entry.name.split('.').pop().toLowerCase())) { ++report.filtered; continue; }
      progress('校验：' + entry.name);
      let name, checked;
      try {
        name = safeName(entry.name);
        if (names.has(name)) throw Error('重复文件名');
        names.add(name);
        checked = await inspect(file, entry);
      } catch (error) { report.skipped.push({name: entry.name, flags: entry.flags, error: error.message}); continue; }
      const verified = checked.checksum === entry.checksum;
      const detail = {name, bytes: checked.bytes, flags: entry.flags,
        expected_adler32: entry.checksum.toString(16).padStart(8, '0'), actual_adler32: checked.checksum.toString(16).padStart(8, '0')};
      if (!verified) {
        detail.prefix_hex = checked.prefix_hex;
        detail.error = 'Adler-32 不一致：可能使用自定义哈希、加密或数据损坏；尚未确认原因';
        if (!retainUnverified) { report.skipped.push(detail); continue; }
      }
      progress('导出：' + name);
      // Validate first, then stream again. No whole-file buffer or failed ZIP entry.
      await writer.add((verified ? 'files/' : 'unverified/') + name, entry.size, checked.crc, contents(file, entry));
      report[verified ? 'exported' : 'unverified'].push(detail);
    }
    const bytes = new TextEncoder().encode(JSON.stringify(report, null, 2));
    await writer.add('kirikinux-diagnostic.json', bytes.length, (crcUpdate(0xffffffff, bytes) ^ 0xffffffff) >>> 0,
      (async function* () { yield bytes; })());
    return {result: await writer.close(), report};
  }
  async function exportZip(file, scripts = true, progress = () => {}, retainUnverified = true) {
    const parts = [], sink = {write: async bytes => { parts.push(bytes); }, close: async () => new Blob(parts, {type: 'application/zip'})};
    const {result, report} = await exportToSink(file, sink, scripts, progress, retainUnverified);
    return {blob: result, report};
  }
  async function exportToFile(file, handle, scripts = true, progress = () => {}, retainUnverified = true) {
    if (handle.name === file.name) throw Error('不能覆盖原 XP3');
    const writable = await handle.createWritable();
    try { return (await exportToSink(file, writable, scripts, progress, retainUnverified)).report; }
    catch (error) { await writable.abort(); throw error; }
  }
  return {index, exportZip, exportToFile, safeName};
})();
if (typeof module !== 'undefined') module.exports = XP3Tool;
if (typeof document !== 'undefined') {
  const file = document.getElementById('file'), status = document.getElementById('status');
  const buttons = ['scripts', 'all'].map(id => document.getElementById(id));
  buttons.forEach((button, i) => button.addEventListener('click', async () => {
    const source = file.files[0];
    if (!source) { status.textContent = '请先选择 XP3 文件。'; return; }
    const name = source.name.replace(/\.xp3$/i, '') + (i === 0 ? '-scripts' : '-unpacked') + '.zip';
    buttons.forEach(b => b.disabled = true);
    try {
      // Obtain the save handle within the user gesture, before asynchronous work.
      const handle = typeof window.showSaveFilePicker === 'function' ? await window.showSaveFilePicker({suggestedName: name,
        excludeAcceptAllOption: true, types: [{description: 'ZIP 诊断包', accept: {'application/zip': ['.zip']}}]}) : null;
      const retain = document.getElementById('unverified').checked;
      const progress = message => { status.textContent = message; };
      let report;
      if (handle) report = await XP3Tool.exportToFile(source, handle, i === 0, progress, retain);
      else {
        const result = await XP3Tool.exportZip(source, i === 0, progress, retain);
        report = result.report;
        const url = URL.createObjectURL(result.blob), link = document.createElement('a');
        link.href = url; link.download = name; link.click();
        setTimeout(() => URL.revokeObjectURL(url), 60000);
      }
      status.textContent = `完成：校验一致 ${report.exported.length} 个，诊断字节 ${report.unverified.length} 个，失败 ${report.skipped.length} 个。详细信息在 kirikinux-diagnostic.json 中；unverified/ 不代表已解密。`;
    } catch (error) { status.textContent = error.name === 'AbortError' ? '已取消保存。' : '提取失败：' + error.message; }
    finally { buttons.forEach(b => b.disabled = false); }
  }));
}
