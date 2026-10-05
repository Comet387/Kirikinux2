#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
import importlib.util, json, pathlib, struct, subprocess, sys, tempfile, unittest, zipfile, zlib
ROOT=pathlib.Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('xp3_extract',ROOT/'tools/xp3-extract.py');xp=importlib.util.module_from_spec(spec);sys.modules[spec.name]=xp;spec.loader.exec_module(xp)
def chunk(name,data):return name+struct.pack('<Q',len(data))+data

def fixture(path):
    payload=bytearray(xp.SIGNATURE+b'\0'*8)
    records=[]; expected={}
    for name,data,compressed,broken in [
        ('startup.tjs',b'\xff\xfe'+ 'var kirikiriz = false;\n'.encode('utf-16-le'),True,False),
        ('System/initialize.tjs',b'TJS2\x00\xff\x81\x00compiled bytecode',False,False),
        ('images/large.png',b'not a script'*100000,True,False),
        ('../escape.tjs',b'bad path',False,False),
        ('encrypted.tjs',b'unreadable ciphertext',False,True)]:
        segments=bytearray()
        for segment in [data[:len(data)//2],data[len(data)//2:]]:
            stored=zlib.compress(segment) if compressed else segment
            segments+=struct.pack('<IQQQ',1 if compressed else 0,len(payload),len(segment),len(stored));payload+=stored
        encoded=name.encode('utf-16-le');info=struct.pack('<IQQH',0x80000000,len(data),sum(struct.unpack_from('<IQQQ',segments,i)[3] for i in range(0,len(segments),28)),len(encoded)//2)+encoded
        records.append(chunk(b'File',chunk(b'info',info)+chunk(b'segm',segments)+chunk(b'adlr',struct.pack('<I',zlib.adler32(data)^(1 if broken else 0)))))
        if not broken and '..' not in name and name.endswith('.tjs'):expected['files/'+name]=data
    # Continuation pointer follows the stored first index, as in the engine.
    first=b''.join(records[:2]);packed=zlib.compress(first);position=len(payload)
    first_header=b'\x81'+struct.pack('<QQ',len(packed),len(first))+packed
    second_position=position+len(first_header)+8
    payload+=first_header+struct.pack('<Q',second_position)
    second=b''.join(records[2:]);payload+=b'\x00'+struct.pack('<Q',len(second))+second
    struct.pack_into('<Q',payload,11,position);path.write_bytes(payload);return expected

class ExportTests(unittest.TestCase):
    def test_script_export_preserves_binary_and_skips_bad_entries(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=pathlib.Path(tmp);src=root/'测试 空格.xp3';expected=fixture(src);original=src.read_bytes();out=root/'scripts.zip'
            report=xp.export(src,out)
            self.assertEqual(2,len(report['exported']));self.assertEqual(1,len(report['skipped']))
            self.assertEqual(1,len(report['unverified']))
            with zipfile.ZipFile(out) as archive:
                for name,data in expected.items():self.assertEqual(data,archive.read(name))
                self.assertNotIn('files/images/large.png',archive.namelist())
                self.assertEqual(b'unreadable ciphertext',archive.read('unverified/encrypted.tjs'))
                self.assertNotIn('files/encrypted.tjs',archive.namelist())
            self.assertEqual(original,src.read_bytes());self.assertFalse((root/'escape.tjs').exists())
            with self.assertRaises(ValueError):xp.export(src,out)
    def test_browser_export_matches_python_zip(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=pathlib.Path(tmp);src=root/'data.xp3';expected=fixture(src);out=root/'browser.zip'
            command='''const fs=require('fs'),tool=require(process.argv[1]);(async()=>{const file=new Blob([fs.readFileSync(process.argv[2])]);file.name='data.xp3';const result=await tool.exportZip(file);fs.writeFileSync(process.argv[3],Buffer.from(await result.blob.arrayBuffer()));console.log(JSON.stringify(result.report));})().catch(e=>{console.error(e);process.exit(1)});'''
            subprocess.run(['node','-e',command,str(ROOT/'tools/xp3-tool.js'),str(src),str(out)],check=True,stdout=subprocess.PIPE)
            with zipfile.ZipFile(out) as archive:
                self.assertIsNone(archive.testzip())
                for name,data in expected.items():self.assertEqual(data,archive.read(name))
                report=json.loads(archive.read('kirikinux-diagnostic.json'));self.assertEqual(1,len(report['skipped']))
                self.assertEqual(b'unreadable ciphertext',archive.read('unverified/encrypted.tjs'))
    def test_verified_only_still_rejects_mismatches(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=pathlib.Path(tmp);src=root/'data.xp3';fixture(src);out=root/'verified.zip'
            report=xp.export(src,out,retain_unverified=False)
            self.assertEqual(2,len(report['skipped']));self.assertFalse(report['unverified'])
            with zipfile.ZipFile(out) as archive:self.assertNotIn('unverified/encrypted.tjs',archive.namelist())
    def test_browser_streams_directly_to_file(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=pathlib.Path(tmp);src=root/'data.xp3';expected=fixture(src);out=root/'stream.zip'
            command='''const fs=require('fs'),tool=require(process.argv[1]);(async()=>{const file=new Blob([fs.readFileSync(process.argv[2])]);file.name='data.xp3';const fd=await fs.promises.open(process.argv[3],'wx');const handle={name:'stream.zip',createWritable:async()=>({write:async bytes=>{await fd.write(bytes)},close:async()=>{await fd.close()},abort:async()=>{await fd.close()}})};await tool.exportToFile(file,handle);})().catch(e=>{console.error(e);process.exit(1)});'''
            subprocess.run(['node','-e',command,str(ROOT/'tools/xp3-tool.js'),str(src),str(out)],check=True)
            with zipfile.ZipFile(out) as archive:
                self.assertIsNone(archive.testzip())
                for name,data in expected.items():self.assertEqual(data,archive.read(name))
                self.assertEqual(b'unreadable ciphertext',archive.read('unverified/encrypted.tjs'))
    def test_script_larger_than_previous_32mib_limit(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=pathlib.Path(tmp);src=root/'large.xp3';out=root/'large.zip'
            size=33*1024*1024;data=b' '*size;packed=zlib.compress(data);name='large.tjs'.encode('utf-16-le')
            info=struct.pack('<IQQH',0,size,len(packed),len(name)//2)+name
            record=chunk(b'File',chunk(b'info',info)+chunk(b'segm',struct.pack('<IQQQ',1,19,size,len(packed)))+chunk(b'adlr',struct.pack('<I',zlib.adler32(data))))
            src.write_bytes(xp.SIGNATURE+struct.pack('<Q',19+len(packed))+packed+b'\x00'+struct.pack('<Q',len(record))+record)
            report=xp.export(src,out)
            self.assertEqual(size,report['exported'][0]['bytes'])
            with zipfile.ZipFile(out) as archive:
                self.assertIsNone(archive.testzip());self.assertEqual(size,archive.getinfo('files/large.tjs').file_size)
            browser=root/'browser-large.zip'
            command='''const fs=require('fs'),tool=require(process.argv[1]);(async()=>{const file=new Blob([fs.readFileSync(process.argv[2])]);file.name='large.xp3';const fd=await fs.promises.open(process.argv[3],'wx');const handle={name:'out.zip',createWritable:async()=>({write:async b=>{await fd.write(b)},close:async()=>{await fd.close()},abort:async()=>{await fd.close()}})};const r=await tool.exportToFile(file,handle);if(r.exported.length!==1)throw Error('large script was skipped');})().catch(e=>{console.error(e);process.exit(1)});'''
            subprocess.run(['node','-e',command,str(ROOT/'tools/xp3-tool.js'),str(src),str(browser)],check=True)
            with zipfile.ZipFile(browser) as archive:
                self.assertIsNone(archive.testzip());self.assertEqual(size,archive.getinfo('files/large.tjs').file_size)
    def test_rejects_bad_index_offset(self):
        with tempfile.TemporaryDirectory() as tmp:
            src=pathlib.Path(tmp)/'bad.xp3';src.write_bytes(xp.SIGNATURE+struct.pack('<Q',999999))
            with self.assertRaises(ValueError):xp.XP3(src)
if __name__=='__main__':unittest.main()
