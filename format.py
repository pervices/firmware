import os

for subdir, dirs, files in os.walk("."):
    for file in files:
        if file.endswith(('.c', '.h')):
            path = os.path.join(subdir, file)
            os.system("clang-format -style='{SortIncludes: false, IndentWidth: 4, ColumnLimit: 80, AllowShortBlocksOnASingleLine: false}' %s" % path)
