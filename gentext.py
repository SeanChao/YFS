f = open('example.txt', 'w')
line_len = 1024
line_num = 400

for i in range(line_num):
    s = str(i % 10) * (line_len - 1) + '\n'
    f.write(s)

