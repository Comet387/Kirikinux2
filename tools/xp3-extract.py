#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""XP3 script diagnostic export / unpacker. No arguments opens a graphical picker.
Format reference: src/core/base/XP3Archive.cpp in this repository.
Supports standard raw/zlib indexes and segmented files; no game-specific decryptor.
"""
import argparse
from dataclasses import dataclass
import json
from pathlib import Path, PurePosixPath
import struct
import tempfile
import zipfile
import zlib

SIGNATURE = b'XP3\r\n \n\x1a\x8bg\x01'
SCRIPT_TYPES = {'.tjs', '.ks', '.scn', '.ini', '.cfg', '.txt', '.xml', '.json', '.csv', '.asd', '.tpm', '.dll'}

@dataclass
class Entry:
    name: str
    flags: int
    size: int
    checksum: int
    segments: list

def exact(file, size):
    if size < 0: raise ValueError('索引/缓冲区大小异常')
    data = file.read(size)
    if len(data) != size: raise ValueError('XP3 数据不完整')
    return data

def chunks(data):
    pos = 0
    while pos < len(data):
        if pos + 12 > len(data): raise ValueError('XP3 索引块头损坏')
        name, size = struct.unpack_from('<4sQ', data, pos); pos += 12
        if size > len(data) - pos: raise ValueError('XP3 索引块长度损坏')
        yield name, data[pos:pos + size]
        pos += size

def inflate(data, size):
    decoder = zlib.decompressobj()
    result = decoder.decompress(data, size + 1)
    if len(result) != size or not decoder.eof or decoder.unused_data:
        raise ValueError('压缩索引长度/格式不正确')
    return result

class XP3:
    def __init__(self, path):
        self.path = Path(path)
        self.entries = []
        total_size = self.path.stat().st_size
        with self.path.open('rb') as file:
            if exact(file, 11) != SIGNATURE: raise ValueError('不是标准 XP3；游戏专用加密索引暂不支持')
            position = struct.unpack('<Q', exact(file, 8))[0]
            visited = set()
            while True:
                if position in visited or position >= total_size: raise ValueError('索引位置异常')
                visited.add(position); file.seek(position)
                flags = exact(file, 1)[0]
                stored = struct.unpack('<Q', exact(file, 8))[0]
                if stored > total_size - file.tell(): raise ValueError('索引超出 XP3 文件范围')
                if flags & 7 == 1:
                    size = struct.unpack('<Q', exact(file, 8))[0]
                    index = inflate(exact(file, stored), size)
                elif flags & 7 == 0:
                    index = exact(file, stored)
                else: raise ValueError('暂不支持此索引压缩方法')
                next_index = struct.unpack('<Q', exact(file, 8))[0] if flags & 0x80 else None
                for kind, block in chunks(index):
                    if kind != b'File': continue
                    fields = dict(chunks(block))
                    info = fields[b'info']; segm = fields[b'segm']
                    if len(info) < 22 or len(segm) % 28: raise ValueError('文件索引损坏')
                    attr, size, packed, length = struct.unpack_from('<IQQH', info)
                    if 22 + length * 2 > len(info): raise ValueError('文件名索引损坏')
                    name = info[22:22 + length * 2].decode('utf-16-le')
                    segments = [struct.unpack_from('<IQQQ', segm, i) for i in range(0, len(segm), 28)]
                    if sum(s[2] for s in segments) != size or sum(s[3] for s in segments) != packed:
                        raise ValueError('分段长度与文件长度不一致')
                    for method, offset, original, archived in segments:
                        if method & 7 not in (0, 1) or offset > total_size or archived > total_size - offset:
                            raise ValueError('分段位置/压缩方法不正确')
                        if not (method & 7) and original != archived: raise ValueError('未压缩分段长度不一致')
                    self.entries.append(Entry(name, attr, size, struct.unpack('<I', fields[b'adlr'])[0], segments))
                if next_index is None: break
                position = next_index

    def stream(self, entry, target):
        checksum = 1
        with self.path.open('rb') as file:
            for flags, offset, size, stored in entry.segments:
                file.seek(offset); remaining = stored; written = 0
                decoder = zlib.decompressobj() if flags & 7 else None
                while remaining:
                    data = exact(file, min(65536, remaining)); remaining -= len(data)
                    pending = data
                    while pending:
                        output = decoder.decompress(pending, 65536) if decoder else pending
                        pending = decoder.unconsumed_tail if decoder else b''
                        written += len(output)
                        if written > size: raise ValueError('解压后的分段超过声明大小')
                        checksum = zlib.adler32(output, checksum)
                        target.write(output)
                if written != size or (decoder and (not decoder.eof or decoder.unused_data)):
                    raise ValueError('分段解压失败：可能需要游戏专用解密补丁')
        return checksum & 0xffffffff

def safe_name(name):
    name = name.replace('\\', '/')
    parts = PurePosixPath(name)
    if not parts.parts or '\x00' in name or parts.is_absolute() or '..' in parts.parts or ':' in name:
        raise ValueError('不安全的归档路径：' + repr(name))
    return str(parts)

def export(path, destination, scripts=True, progress=lambda _: None, retain_unverified=True):
    """Writes a new ZIP. Original XP3 is never changed; failures listed in report."""
    archive = XP3(path)
    destination = Path(destination)
    if destination.exists(): raise ValueError('输出文件已存在，请选择新文件名')
    if destination.resolve() == Path(path).resolve(): raise ValueError('不能覆盖原 XP3')
    report = {'archive': Path(path).name, 'mode': 'scripts' if scripts else 'all',
              'entries': len(archive.entries), 'exported': [], 'unverified': [], 'skipped': [], 'filtered': 0,
              'notes': 'files/ 为校验一致的内容；unverified/ 为校验不一致的诊断字节，可能是自定义哈希、加密或损坏，不能据此断言密文。不实现游戏专用解密。' }
    names = set()
    # Write an atomic result; an interrupted export never leaves a partial ZIP.
    with tempfile.TemporaryDirectory(dir=destination.parent) as temporary:
        pending = Path(temporary) / 'output.zip'
        with zipfile.ZipFile(pending, 'w', zipfile.ZIP_DEFLATED, allowZip64=True) as zip:
            for entry in archive.entries:
                if scripts and Path(entry.name).suffix.lower() not in SCRIPT_TYPES:
                    report['filtered'] += 1
                    continue
                progress(entry.name)
                try:
                    name = safe_name(entry.name)
                    if name in names: raise ValueError('重复文件名')
                    names.add(name)
                    with tempfile.TemporaryFile(dir=temporary) as output:
                        checksum = archive.stream(entry, output)
                        verified = checksum == entry.checksum
                        detail = {'name': name, 'bytes': entry.size, 'flags': entry.flags,
                                  'expected_adler32': f'{entry.checksum:08x}', 'actual_adler32': f'{checksum:08x}'}
                        if not verified:
                            output.seek(0)
                            detail['prefix_hex'] = output.read(32).hex()
                            detail['error'] = 'Adler-32 不一致：可能使用自定义哈希、加密或数据损坏；尚未确认原因'
                            if not retain_unverified:
                                report['skipped'].append(detail)
                                continue
                        output.seek(0)
                        folder = 'files/' if verified else 'unverified/'
                        with zip.open(folder + name, 'w', force_zip64=True) as target:
                            while data := output.read(65536): target.write(data)
                    report['exported' if verified else 'unverified'].append(detail)
                except (ValueError, KeyError, zlib.error, UnicodeError) as error:
                    report['skipped'].append({'name': entry.name, 'error': str(error), 'flags': entry.flags})
            zip.writestr('kirikinux-diagnostic.json', json.dumps(report, ensure_ascii=False, indent=2))
        pending.replace(destination)
    return report

def gui():
    import tkinter as tk
    from tkinter import filedialog, messagebox
    import threading, queue
    window = tk.Tk(); window.title('Kirikinux2 · XP3 提取工具'); window.geometry('640x370')
    tk.Label(window, text='提取脚本，方便诊断启动错误', font=('', 16)).pack(pady=14)
    tk.Label(window, text='脚本模式跳过图片、声音和视频，生成的 ZIP 可直接附在问题反馈中。\n保留 TJS 字节码；原 XP3 不会被修改。\n校验异常可保留诊断字节；游戏专用加密仍需对应补丁。', justify='left').pack(padx=20)
    status = tk.StringVar(value='选择本地 XP3 文件即可开始。'); tk.Label(window,textvariable=status,wraplength=540).pack(pady=12)
    retain=tk.BooleanVar(value=True)
    tk.Checkbutton(window,text='保留校验不一致的诊断字节（放入 unverified/，不代表已解密）',variable=retain).pack()
    events=queue.Queue(); controls=[]
    def start(scripts):
        source=filedialog.askopenfilename(title='选择 XP3',filetypes=[('XP3','*.xp3'),('全部文件','*')])
        if not source: return
        output=filedialog.asksaveasfilename(title='保存提取结果',initialfile=Path(source).stem+('-scripts' if scripts else '-unpacked')+'.zip',defaultextension='.zip')
        if not output: return
        for button in controls: button.config(state='disabled')
        status.set('正在读取…')
        keep_unverified=retain.get()
        def work():
            try: events.put(('done',export(source,output,scripts,lambda name: events.put(('progress',name)),keep_unverified)))
            except Exception as error: events.put(('error',str(error)))
        threading.Thread(target=work,daemon=True).start()
    for label,scripts in [('选择 XP3 并导出脚本包',True),('选择 XP3 并解包全部资源',False)]:
        button=tk.Button(window,text=label,command=lambda choice=scripts:start(choice));button.pack(pady=3);controls.append(button)
    def poll():
        while not events.empty():
            kind,data=events.get()
            if kind=='progress': status.set('提取：'+data)
            else:
                for button in controls: button.config(state='normal')
                if kind=='error': messagebox.showerror('提取失败',data)
                else: messagebox.showinfo('导出完成',f"校验一致 {len(data['exported'])} 个，诊断字节 {len(data['unverified'])} 个，失败 {len(data['skipped'])} 个。\n请把生成的 ZIP 发来。")
                status.set('可以继续选择其他 XP3。')
        window.after(100,poll)
    poll();window.mainloop()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive',nargs='?',type=Path)
    parser.add_argument('--output',type=Path)
    parser.add_argument('--all',action='store_true',help='提取全部资源（默认仅脚本）')
    parser.add_argument('--list',action='store_true')
    parser.add_argument('--verified-only',action='store_true',help='只导出校验一致的文件，不保留诊断字节')
    args=parser.parse_args()
    if args.archive is None: gui();return
    if args.list:
        print(json.dumps([vars(e) for e in XP3(args.archive).entries],ensure_ascii=False,indent=2));return
    output=args.output or args.archive.with_name(args.archive.stem+'-scripts.zip')
    result=export(args.archive,output,not args.all,retain_unverified=not args.verified_only)
    print(json.dumps({'output':str(output),'exported':len(result['exported']),'unverified':len(result['unverified']),'skipped':result['skipped']},ensure_ascii=False))
if __name__=='__main__': main()
