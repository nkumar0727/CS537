import sys
import csv
import matplotlib.pyplot as plt

def main(argv):
    oneProc = []
    twoProc = []
    threeProc = []
    time = []
    with open('lottery_data.csv', 'r') as csvfile:
        spamreader = csv.reader(csvfile, delimiter=',')
        for row in spamreader:
            #  print(row)
            time.append(row[1])
            oneProc.append(row[2])
            twoProc.append(row[3])
            threeProc.append(row[4])
    a = plt.plot(time, oneProc, "yo") # 10
    b = plt.plot(time, twoProc, "ro")
    c = plt.plot(time, threeProc, "bo")
    plt.title("Time vs. Proccess Ticks")
    plt.legend(["10 tickets", "20 tickets", "30 tickets"], loc="upper left")
    plt.xlabel("Time (global ticks)")
    plt.ylabel("Ticks")
    plt.show()

    
if __name__ == "__main__":
    main(sys.argv)
