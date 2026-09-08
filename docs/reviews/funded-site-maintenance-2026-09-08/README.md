# Funded unattended site maintenance

Related: #393, #391, #394 and parent #396. Base: draft #515 at `ee5e861`.

Open recipe-bearing sites below condition 80 can spend their weekly work on one repair. It uses one Tools bundle and one Wood from that site's store and restores ten condition points. The crew preserves one working Tools bundle and the production recipe's Wood reserve. The site stops repairing when it reaches at least 80. The same work recipe defines player and site repair costs; the site adds its local reserve policy.

A funded repair consumes the site's weekly work slot. Its ordinary production preview reports work as occupied. When repair materials are short, the ordinary production rules determine whether the site can work. Road houses remain outside recipe maintenance.

Existing royal carriages can deliver repair supplies to an open site below the production threshold. Maintenance orders request two Tools and one Wood above the local production reserve. Town reserves, actual cargo slots, road capacity and elapsed journey time still apply. Both planned pickups and return-leg pickups retain repair materials at damaged works. The home-town identity selects the supplier; goods move through shipments into the local store before use.

## Persistence and accounting

Simulation schema 67 enables unattended maintenance and its freight policy. Generator 25, SQLite schema 31 and the saved field layout remain unchanged. Schema 66 journal suffixes replay with the earlier policy, then upgrade. Existing condition and stock fields retain the result.

JSON protocol 5 adds `maintenance_input`, `maintenance_work` and `site_repair` per site. Production input/output and freight remain separate. Physical conservation uses initial stock plus received freight and output, less shipped freight, production input and maintenance input. Maintenance work records two units per repair; condition gain records ten.

Use `make production-baseline` for the common four-case, repeated 40-year Release report. Wear remains the next step after this funded maintenance path.

## Tests

`funded_site_maintenance` checks four weekly repairs from condition 44 to 84, exact local costs, occupied work, the following Bread batch, daily/annual observer parity, saved condition, missing funds, access gates and schema 66 behavior. A separate fixture starts with an empty mill store, waits for actual carriage arrivals, and verifies funded repairs and material conservation over a year.

The persistence suite checks a schema 66 journal crossing the new maintenance boundary. Shared road-production accounting tests include repair consumption in their site-stock equations. JSON capture checks repeatability and the same conservation equation.
