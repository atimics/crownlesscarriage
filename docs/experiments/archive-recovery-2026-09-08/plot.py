from pathlib import Path
import json
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
root=Path(__file__).resolve().parent
rows=[json.loads(line) for line in (root/'probe.jsonl').read_text().splitlines()]
control=next(r for r in rows if r['kind']=='window' and r['label']=='control')
fig,axes=plt.subplots(1,2,figsize=(10,4),layout='constrained')
axes[0].bar(['Before','After'],[0,control['days_with_new_heard_gossip']],color=['#b98656','#427f92'])
axes[0].set_title('Seed 2: days receiving new reports')
axes[0].set_ylabel('Days in the next 100 years')
axes[1].bar(['Before','After'],[24455/365,100],color=['#b98656','#427f92'])
axes[1].set_title('Archive aid experiment: valid years')
axes[1].set_ylabel('Years observed before failure or completion')
fig.savefig(root/'recovery.png',dpi=160)
