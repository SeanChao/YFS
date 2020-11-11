import re
import textwrap

# f = open('out.log')

# lines = f.readlines()
# target = ''
# for line in lines:
#     if 'error get large file' in line:
#         target = line

# out = re.search(r'(\w+) <-> (\w+)', target)
# print(out)
# print(out[1])
# print(out[2])
out = ['', ''];
out[0] = open('my.txt').read()
out[1] = open('ref.txt').read()
# print(out)


def i_wrap(s):
    return '\n'.join(textwrap.wrap(s, width=512))

print(len(out[0]))
print(len(out[1]))
open('my.f.txt', 'w').write(i_wrap(out[0]))
open('ref.f.txt', 'w').write(i_wrap(out[1]))

