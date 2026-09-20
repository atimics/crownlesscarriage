"""Language realization for typed policy acts; meanings stay in policy.py."""
import hashlib
import subprocess
from policy import validate
from event_facts import render_fact_act

FORMS = {
 'request_help': ('Could you help me find a way through this?', 'I could use some help.'),
 'explain_need': ('I am hungry. Can you help me arrange food?', 'I need food before I can do more work.'),
 'offer_help': ('I can offer {cost} crown toward your food.', 'Could I offer {cost} crown toward something to eat?'),
 'offer_exchange': ('Could we help each other with the work?', 'I can offer my time if we share the work.'),
 'offer_trade': ('I can pay {cost} crowns for your help with the work.', 'Would {cost} crowns buy your help?'),
 'counter_offer': ('Would you pay {cost} crown for my help instead?', 'I would ask {cost} crown for my part.'),
 'explain_price': ('I want the pay to match the work we agree on.', 'We should settle the work as well as the price.'),
 'warn': ('I am afraid. We should think about shelter.', 'I feel unsafe here. Can we find shelter?'),
 'seek_shelter': ('Let us look for shelter together.', 'Could we move somewhere safer together?'),
 'offer_escort': ('I can go with you while we look for shelter.', 'You can have my company on the way.'),
 'daylight': ('Only if we go in daylight.', 'Let us wait for daylight before we go.'),
 'ask_feeling': ('How are you holding up?', 'Would you like to tell me how you feel?'),
 'comfort': ('I hear you. Take the time you need.', 'You can speak with me, if it helps.'),
 'offer_company': ('I can stay with you for a while.', 'Would some company help?'),
 'express_grief': ('I grieve for the losses in this account.', 'This account of loss weighs on me.'),
 'recall_loss': ('I keep returning to this account of loss.', 'I remember hearing about this loss.'),
 'ask_space': ('I need some quiet for now.', 'Please give me a little space.'),
 'share_memory': ('I remember {memory}.', 'One thing stays with me: {memory}.'),
 'thank': ('I am grateful for the help I received.', 'That help still matters to me.'),
 'apologise': ('I am sorry that the help fell through.', 'I regret that the player withdrew that help.'),
 'promise': ('I can offer my time to help rebuild our trust.', 'Let us make a small promise we can keep: time to help.'),
 'grievance': ('There is trouble between us that I want to settle.', 'I want to speak about the strain between us.'),
 'explain_motive': ('I am trying to protect my own needs too.', 'I want a way forward that we can both manage.'),
 'demand_repair': ('Would you offer {cost} crown toward making things right?', 'I ask for {cost} crown as a first step toward repair.'),
 'forgive': ('I am willing to make a fresh start.', 'Let us try to rebuild our trust.'),
 'request_tribute': ('I ask for {cost} crown as a contribution to our faction.', 'Will you contribute {cost} crown to the faction?'),
 'invoke_duty': ('I feel an obligation here. Can we agree how to meet it?', 'We should talk about the duty between us.'),
 'bargain': ('I can offer {cost} crown toward that duty.', 'Would {cost} crown settle my part for now?'),
 'refuse_duty': ('I will refuse that demand.', 'I will take no part in that duty.'),
 'threaten': ('Keep pressing this dispute and I may turn against you.', 'If this dispute goes on, expect resistance from me.'),
 'ask_fact': ('What have you heard that might help us?', 'Do you have an account we could learn from?'),
 'report_fact': ('{fact}', '{fact}'),
 'explain_cause': ('This account is why I am concerned: {fact}', 'My concern comes from this account: {fact}'),
 'uncertain': ('I have no account I can share about that.', 'I am unsure; I need an account I can check.'),
 'accept': ('I agree to that proposal.', 'Those terms work for me.'),
 'decline': ('I will pass on that proposal.', 'I cannot agree to those terms.'),
 'acknowledge': ('I understand. Thank you for telling me.', 'I will keep that in mind.'),
 'end': ('Goodbye for now.', 'We can speak again later.'),
}
MEMORIES = {1: 'meeting the player', 2: 'a promise from the player',
            3: 'help from the player', 4: 'the player withdrawing'}


def render(act, person, heard, requested=None, language='human', probe=None, variant=0):
    validate(act, person, heard, requested)
    fact = ''
    if act['claim']:
        fact = render_fact_act({'kind': 'report', 'fact_ref': act['claim']['ref']}, person, person['listener']['id'])
    text = FORMS[act['intent']][variant % 2].format(
        cost=(act['proposal'] or {}).get('cost', 0), fact=fact,
        memory=MEMORIES.get((act['memory'] or {}).get('kind'), 'an earlier meeting'))
    if act['intent'] in ('express_grief', 'recall_loss'): text += ' ' + fact
    if language == 'human': return text
    if language != 'goblin' or probe is None: raise ValueError('goblin rendering needs the native probe')
    # Translate authored wording separately, preserving the attributed account verbatim.
    # The account can contain names whose spelling the literal renderer cannot identify.
    if fact:
        before, after = text.split(fact, 1)
        translate = lambda s: subprocess.check_output([str(probe), '--literal', '100', s], text=True).rstrip('\n')
        return translate(before) + fact + translate(after)
    return subprocess.check_output([str(probe), '--literal', '100', text], text=True).rstrip('\n')
