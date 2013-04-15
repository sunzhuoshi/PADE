i = 0
fileBytesList = []


def writeFileBytesList():
    w = open("WAVE" + str(i) + ".wav", "wb")
    for b in fileBytesList:
        w.write(b)
    w.close()

with open("Data003.BIN", "rb") as f:
    fileBytes = f.read(512)
    while fileBytes != "":
        if fileBytes[0] == 'R' and fileBytes[1] == 'I' and fileBytes[2] == 'F' and fileBytes[3] == 'F':
            if len(fileBytesList) > 0:
                writeFileBytesList()
                fileBytesList = []
                i += 1
        fileBytesList.append(fileBytes)
        fileBytes = f.read(512)
    writeFileBytesList()



