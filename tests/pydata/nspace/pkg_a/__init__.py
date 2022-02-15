# @mindmaze_header@


class NSpacePkgAData:
    def __init__(self, arg1, arg2):
        self._arg1 = arg1
        self.public = arg2

    def show(self):
        print(self._arg1)


def nspace_pkga_func(data: NSpacePkgAData):
    data.show()
