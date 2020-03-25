# @mindmaze_header@
"""
Source build event using Gerrit 
"""

from eventsrc import EventSource
from gerrit import Gerrit


def _trigger_build(event):
    """
    test whether an event is a merge event, or a manual trigger
    """
    try:
        do_build = (event['type'] == 'change-merged'
                    or (event['type'] == 'comment-added'
                        and ('MMPACK_BUILD' in event['comment']
                             or 'MMPACK_UPLOAD_BUILD' in event['comment'])))
        do_upload = (event['type'] == 'change-merged'
                     or (event['type'] == 'comment-added'
                         and 'MMPACK_UPLOAD_BUILD' in event['comment']))

        return do_build, do_upload
    except KeyError:
        return False


class GerritEventSource(EventSource):
    def __init__(self, jobqueue: Queue):
