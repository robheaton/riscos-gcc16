import json,difflib
d=json.load(open('service-results/port_shcfg2.json'))
out=''.join(d['output']).replace('\r','')
lines=out.split('\n')
# drop the harness header lines
lines=[l for l in lines if not l.startswith('*** ')]
while lines and lines[-1]=='': lines.pop()
exp=open('runcfgs.expected').read().rstrip('\n').split('\n')
diff=list(difflib.unified_diff(exp,lines,'dash','sh',lineterm='',n=1))
print('rc',d['rc'],len(exp),len(lines),'IDENTICAL' if not diff else '%d diff lines'%len(diff))
for l in diff[:50]: print('  ',l)
