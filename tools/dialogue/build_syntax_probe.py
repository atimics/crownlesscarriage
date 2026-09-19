"""Build a native probe for a supplied syntax checkpoint, with source receipts."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build(model, libraries, output):
    root=Path(__file__).resolve().parents[2]
    output.mkdir(parents=True,exist_ok=False)
    receipt={'status':'running','model_sha256':sha(model),'builder_sha256':sha(Path(__file__))}
    try:
        compiler=shutil.which('cc')
        if compiler is None:
            raise ValueError('C compiler required')
        module_path=root/'tools/language/compile_model.py'
        spec=importlib.util.spec_from_file_location('syntax_native_tables',module_path)
        tables=importlib.util.module_from_spec(spec);spec.loader.exec_module(tables)
        for folder in ('assets/language','tools/language','story'):
            (output/folder).mkdir(parents=True)
        shutil.copyfile(model,output/'assets/language/core.ccv2')
        shutil.copyfile(root/'assets/language/tokenizer.json',output/'assets/language/tokenizer.json')
        shutil.copyfile(root/'tools/language/unicode_classes.json',output/'tools/language/unicode_classes.json')
        tables.ROOT=output;tables.MODEL_SHA=sha(model)
        tables.TOKENIZER_SHA=sha(output/'assets/language/tokenizer.json')
        generated=output/'story/cc_core_model_tables.inc'
        generated.write_text(tables.compile_tables())
        sources=[root/'tools/core_model_probe.c',root/'src/story/cc_core_model.c',
                 libraries/'libcrownless_story.a',libraries/'libcrownless_sim.a']
        command=[compiler,'-std=c11','-O2','-I',str(output),'-I',str(root/'src'),
                 *map(str,sources),'-lm','-o',str(output/'probe')]
        receipt.update(command=command,tokenizer_sha256=tables.TOKENIZER_SHA,
                       tables_sha256=sha(generated),sources={str(p):sha(p) for p in sources+[module_path]})
        result=subprocess.run(command,capture_output=True,timeout=60)
        (output/'build.stdout').write_bytes(result.stdout)
        (output/'build.stderr').write_bytes(result.stderr)
        receipt['returncode']=result.returncode
        if result.returncode:
            raise ValueError('native probe build failed')
        receipt.update(status='complete',probe_sha256=sha(output/'probe'))
    except BaseException as error:
        receipt.update(status='failed',error=repr(error))
        raise
    finally:
        (output/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    return receipt


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('model','build','output'):
        parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args()
    print(json.dumps(build(args.model.resolve(),args.build.resolve(),args.output.resolve())))


if __name__=='__main__':
    main()
