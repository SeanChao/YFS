import re

f = open('extent_server.log')

lines = f.readlines()

buf = []
for l in lines:
    if re.match(r'> im: alloc_inode \d+', l) != None:
        if l in buf:
            print("duplicate: {l}")
            continue
        buf.append(l)
    
# print(buf)

