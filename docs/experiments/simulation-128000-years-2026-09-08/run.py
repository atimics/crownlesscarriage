#!/usr/bin/env python3
"""Sweep old worlds; retain annual block statistics and sampled raw rows."""
import argparse
import concurrent.futures as cf
import csv
import gzip
import hashlib
import json
from pathlib import Path
import subprocess
import time

METRICS = ['total_population','active_settlements','closed_routes',
           'average_prosperity','average_hunger','average_legitimacy',
           'dragon_hoard','market_coins','live_treasures','live_treasure_value',
           'treasures_from_ruins','treasures_in_ruins','lore_stored',
           'archive_stewardship','lore_ceiling','bandit_influence_end',
           'wars','alliances','active_situations','treasure_count']

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('binary',type=Path)
    parser.add_argument('--seeds',type=int,default=32)
    parser.add_argument('--years',type=int,default=128000)
    parser.add_argument('--jobs',type=int,default=8)
    args=parser.parse_args()
    if min(args.seeds,args.years,args.jobs)<1 or args.years%1000:
        parser.error('Use positive counts and a year count divisible by 1000.')
    root=Path(__file__).resolve().parent
    progress=Path('/private/tmp/crownless-long-progress')
    progress.mkdir(exist_ok=True)
    for name in ('annual','blocks','logs'): (root/name).mkdir(exist_ok=True)
    manifest={'source':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
              'simulation_base':'06e5701c6d6c09bc25430685a9fd917b81a42972',
              'seeds':args.seeds,'years':args.years,'jobs':args.jobs,
              'binary_sha256':hashlib.sha256(args.binary.read_bytes()).hexdigest(),
              'sampling':'Every year through 1000, then every 100 years; all annual observations feed 1000-year blocks.',
              'runs':[]}
    start=time.monotonic()
    def run(seed):
        command=[str(args.binary),'--seed',str(seed),'--years',str(args.years),'--campaign-metrics']
        begin=time.monotonic(); last=None; previous=None; total=0; block=[]
        block_stats={k:[] for k in METRICS}; identity_changes=0
        gold_first=None; gold_constant=True
        sampled=root/'annual'/f'seed-{seed:03}.csv.gz'
        blocks=root/'blocks'/f'seed-{seed:03}.csv.gz'
        with open(root/'logs'/f'seed-{seed:03}.log','w') as errors, gzip.open(sampled,'wt',newline='') as out, gzip.open(blocks,'wt',newline='') as bout:
            process=subprocess.Popen(command,stdout=subprocess.PIPE,stderr=errors,text=True)
            reader=csv.DictReader(process.stdout)
            writer=csv.DictWriter(out,fieldnames=reader.fieldnames,lineterminator='\n'); writer.writeheader()
            bwriter=csv.DictWriter(bout,fieldnames=['seed','start_year','end_year','metric','mean','minimum','maximum','stddev','changes'],lineterminator='\n');bwriter.writeheader()
            block_changes={k:0 for k in METRICS}
            for raw in reader:
                row={k:int(v) for k,v in raw.items()}
                total+=1; year=row['year']
                if year!=total: raise ValueError(f'Seed {seed}: unexpected year {year}')
                if gold_first is None:gold_first=row['tracked_gold']
                gold_constant &= row['tracked_gold']==gold_first
                if year<=1000 or year%100==0:writer.writerow(raw)
                for k in METRICS:
                    block_stats[k].append(row[k])
                    if previous is not None and row[k]!=previous[k]:block_changes[k]+=1
                if previous is not None and row['treasure_identity_hash']!=previous['treasure_identity_hash']:identity_changes+=1
                previous=row;last=row
                if year%1000==0:
                    for k,values in block_stats.items():
                        mean=sum(values)/len(values)
                        bwriter.writerow(dict(seed=seed,start_year=year-999,end_year=year,metric=k,mean=mean,minimum=min(values),maximum=max(values),stddev=(sum((x-mean)**2 for x in values)/len(values))**.5,changes=block_changes[k]))
                    bwriter.writerow(dict(seed=seed,start_year=year-999,end_year=year,metric='treasure_identity_changes',mean=identity_changes/1000,minimum=0,maximum=1,stddev='',changes=identity_changes))
                    block_stats={k:[] for k in METRICS};block_changes={k:0 for k in METRICS};identity_changes=0
                    (progress/f'{seed:03}.json').write_text(json.dumps({'seed':seed,'year':year,'seconds':time.monotonic()-begin}))
            process.stdout.close();code=process.wait()
        return dict(seed=seed,command=command,returncode=code,annual_rows=total,
                    passed=code==0 and total==args.years,seconds=time.monotonic()-begin,
                    gold_constant=gold_constant,endpoint=last)
    with cf.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures=[pool.submit(run,s) for s in range(1,args.seeds+1)]
        for future in cf.as_completed(futures):
            result=future.result();manifest['runs'].append(result)
            (root/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
            print(f"Seed {result['seed']}: {result['annual_rows']} years, passed={result['passed']}, {result['seconds']:.1f}s",flush=True)
    manifest['runs'].sort(key=lambda x:x['seed']);manifest['seconds']=time.monotonic()-start
    manifest['data_hashes']={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for folder in ('annual','blocks') for p in sorted((root/folder).glob('*.gz'))}
    (root/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(f"Finished: {sum(x['passed'] for x in manifest['runs'])}/{args.seeds} passed in {manifest['seconds']:.1f}s",flush=True)

if __name__=='__main__':main()
