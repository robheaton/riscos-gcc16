import json,sys,difflib,re
d=json.load(open('service-results/port_shconf.json'))
out=''.join(d['output']).replace('\r','')
parts=re.split(r'^\*\*\* /sh (s\d\d)\n',out,flags=re.M)
res={}
for i in range(1,len(parts),2): res[parts[i]]=parts[i+1]
tot=0
for s in sorted(res):
    exp=open('../sh-conf/%s.expected'%s).read().rstrip('\n').split('\n')
    got=res[s].rstrip('\n').split('\n')
    if got and got[-1].startswith('*** Done'): got=got[:-1]
    while got and got[-1]=='': got.pop()
    diff=list(difflib.unified_diff(exp,got,'dash','sh',lineterm='',n=0))
    print(s, 'IDENTICAL' if not diff else '%d diff lines'%len(diff))
    for l in diff[:40]: print('   ',l)
