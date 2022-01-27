import sys

def getFilePaths(file_prefix, num_files):
    files = [None] * num_files

    for i in range(num_files):
        files[i] = file_prefix + str(i) + ".txt"

    return files

def getLines(files):
    lines = [None] * len(files)

    for i, file_path in enumerate(files):
        with open(file_path) as f:
            lines[i] = f.readlines()
            lines[i] = list(map(lambda line: line.strip(), lines[i]))

    return lines

def getLeft(line):
    return line.split(": ")[0]

def getRight(line):
    return line.split(": ")[1]

def isFloat(s):
    try:
        float(s)
        return True
    except ValueError:
        return False

def isQueryLine(line):
    if line.find(": ") >= 0:
        right = getRight(line)
        if isFloat(right):
            return True

    return False


def findAverageForLine(lines, line_number):
    num_files = len(lines)
    total = 0
    count = 0

    left0 = getLeft(lines[0][line_number])
    for i in range(num_files):
        left = getLeft(lines[i][line_number])
        if isQueryLine(lines[i][line_number]) and left0 == left:
            right = getRight(lines[i][line_number])
            total += float(right)
            count += 1.0

    return total / float(count)


def calcAverage(file_prefix, num_files, output_file):
    files = getFilePaths(file_prefix, num_files)

    lines = getLines(files)

    out = open(output_file, "w")
    
    for i in range(len(lines[0])):
        if isQueryLine(lines[0][i]):
            left = getLeft(lines[0][i])
            avg = findAverageForLine(lines, i)
            out.write(left + ": " + str(avg) + "\n")
        else:
            out.write(lines[0][i] + "\n")

if __name__ == '__main__':
    file_prefix = sys.argv[1]
    num_files = int(sys.argv[2])
    output_file = sys.argv[3]

    calcAverage(file_prefix, num_files, output_file)
