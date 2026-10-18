import json,difflib
d=json.load(open('service-results/port_sh2s.json'))
out=''.join(d['output'])
got=[l.rstrip('\r') for l in out.split('\n') if not l.startswith('*** ')]
exp=open('t2.expected').read().rstrip('\n').split('\n')
while got and got[-1]=='': got.pop()
diff=list(difflib.unified_diff(exp,got,'dash','sh',lineterm='',n=1))
print(len(exp),len(got)); print('\n'.join(diff[:80]) if diff else 'IDENTICAL')
