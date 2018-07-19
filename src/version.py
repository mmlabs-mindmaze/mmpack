# @mindmaze_header@
'''
TODOC
'''

import yaml
from common import mm_representer


class Version(object):  # pylint: disable=too-few-public-methods
    'version manipulation utility'
    def __init__(self, v_str: str):
        '''
        validate this is in the format x.y.z with digits only
        :raise: ValueError if x or y or z not an integer
        :raise: IndexError if not
        '''
        nums = v_str.split('.')
        if not all(num.isdigit() for num in nums) or len(nums) != 3:
            raise ValueError('Invalid version format: {0}'.format(v_str))

        self.maj = int(nums[0])
        self.min = int(nums[1])
        self.rev = int(nums[2])

    def __lt__(self, other):
        return (self.maj < other.maj
                or (self.maj == other.maj and self.min < other.min)
                or (self.maj == other.maj and
                    self.min == other.min and
                    self.rev < other.rev))

    def __le__(self, other):
        return (self.maj <= other.maj
                or (self.maj == other.maj and self.min <= other.min)
                or (self.maj == other.maj and
                    self.min == other.min and
                    self.rev <= other.rev))

    def __eq__(self, other):
        return (self.maj == other.maj
                or self.min == other.min
                or self.rev == other.rev)

    def __ne__(self, other):
        return not self.__eq__(other)

    def __gt__(self, other):
        return not self.__le__(other)

    def __ge__(self, other):
        return not self.__lt__(other)

    def __repr__(self):
        'as a unicode string again'
        return u'{0}.{1}.{2}'.format(self.maj, self.min, self.rev)

    def __str__(self):
        return self.__repr__()


# add yaml representation of Version as a unicode string
yaml.add_representer(Version, mm_representer)
