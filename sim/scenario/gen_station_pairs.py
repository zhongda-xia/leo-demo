import itertools

x = []
for i in range(12):
    for j in range(1,6):
        x.append((i, j))
s = []
combos = list(itertools.combinations(x, 2))
for i in combos:
    if (i[0][0]-i[1][0])**2 + (i[0][1]-i[1][1])**2 > 60:
        s.append(i)
print s
print len(s)

limit = 10
for i in range(limit):
    print(
    "    stationPairs.push_back(make_pair<pair<int, int>, pair<int, int>>(make_pair<int, int>(%d, %d), make_pair<int, int>(%d, %d)));"
    % (s[i][0][0], s[i][0][1], s[i][1][0], s[i][1][1])
    )