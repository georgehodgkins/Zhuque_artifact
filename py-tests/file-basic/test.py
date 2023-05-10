import math

outfile = open("test_out.txt", 'w')

with open("test_in.txt") as file:
    while instr := file.readline().rstrip():
        innum = float(instr)
        outnum = math.sin(innum)
        print(outnum, file=outfile)
        print("Read input number:", innum, "Wrote output number:", outnum)


outfile.close()
