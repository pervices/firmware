import os

for subdir, dirs, files in os.walk("."):
    for file in files:
        if file.endswith(('.c', '.h')):
            path = os.path.join(subdir, file)
            if "build" in path:
                print("skipping " + path)
                continue
            else:
                print("formatting " + path)
                os.system("clang-format -i -style='{SortIncludes: false, IndentWidth: 4, ColumnLimit: 80, AllowShortBlocksOnASingleLine: false}' %s" % path)
