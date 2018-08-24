import os

for subdir, dirs, files in os.walk("."):
    for file in files:
        if file.endswith(('.c', '.h')):
            path = os.path.join(subdir, file)
            if "build" in path:
                continue
            else:
                os.system("clang-format -i -style='{SortIncludes: false, IndentWidth: 4, ColumnLimit: 80, AllowShortBlocksOnASingleLine: false}' %s" % path)
