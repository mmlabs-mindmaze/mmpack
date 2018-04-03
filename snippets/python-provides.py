#!/usr/bin/env python3

#
# python import:
# cat $file | git grep import *.py
#           | awk -F':' '{print $3}'
#           | sed -n -e 's/^import \(.*\)/\1/p' -e 's/^from \([^ ]*\).*/\1/p'
#           | sort -u
#

import ast
import sys

def top_level_classes(body):
    return (f for f in body if isinstance(f, ast.ClassDef))

def top_level_functions(body):
    return (f for f in body if isinstance(f, ast.FunctionDef))

def parse_ast(filename):
    with open(filename, "rt") as file:
        return ast.parse(file.read(), filename=filename)

if __name__ == "__main__":
    for filename in sys.argv[1:]:
        tree = parse_ast(filename)
        for func in top_level_classes(tree.body):
            print("%s" % func.name)
        for func in top_level_functions(tree.body):
            print("%s" % func.name)
