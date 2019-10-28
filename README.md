# PNG command line tools
## Two tools are contained in this project: findpng and catpng

findpng recurses from a base location in a Unix filesystem up to a tree depth of 6 to find real png images based on metadata contained in the file itself
usage is ./findpng <ROOT_DIRECTORY>
this tool will output all real PNG's found relative to the root path given

catpng takes a series of PNG image strips and concatenates them together vertically to produce a whole PNG image of all the strip
usage is ./catpng /path/to/png_1 /path/to/png_2 ... /path/to/png_n
this tool will output a complete PNG image of all the strips pasted together in the current directory under the name all.png

these tools are best used with each other sequentially:

./catpng $(./findpng <ROOT_DIRECTORY>)

