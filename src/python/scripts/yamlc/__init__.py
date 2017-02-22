#!/usr/bin/python
# -*- coding: utf-8 -*-
# author:   Jan Hybs
# ----------------------------------------------
from scripts.core.base import Paths
# ----------------------------------------------

"""
{
    proc=[1],                   # array of int, where each int represents
                                # number of cores which will be used
    inputs=['input'],           # array (or single string) of string specifying input location
                                # argument (-i) for flow123d. Path can be absolute
                                # or relative (towards yaml file dir)
    time_limit=30,              # number of seconds (give or take resolution) allowed for test to run
    memory_limit=400,           # number of MB allowed for test to use (include all children processes)
    tags=[],                    # array of string, representing tags for case
    check_rules=[               # array of objects, representing check rules
        {
            'ndiff': {          # key is tool which will be used
                'files': ['*']  # regexp matching for files which will be compared using this tool
            }
        }
    ]
}


"""

DEFAULTS = dict(
    proc=[1],
    inputs=['input'],
    time_limit=30,
    memory_limit=400,
    tags=[],
    check_rules=[
        {
            'ndiff': {
                'files': ['*']
            }
        }
    ]
)

TAG_FILES = 'files'
TAG_PROC = 'proc'
TAG_TIME_LIMIT = 'time_limit'
TAG_MEMORY_LIMIT = 'memory_limit'
TAG_TEST_CASES = 'test_cases'
TAG_CHECK_RULES = 'check_rules'
TAG_TAGS = 'tags'
TAG_INPUTS = 'inputs'
REF_OUTPUT_DIR = 'ref_out'
TEST_RESULTS = 'test_results'

YAML = '.yaml'
CONFIG_YAML = 'config.yaml'


class ConfigCaseFiles(object):
    """
    Class ConfigCaseFiles is helper class for defining path for ConfigCase
    """

    def __init__(self, root, ref_output, output, input):
        """
        :type ref_output: str
        :type output: str
        :type root: str
        """
        self.root = root
        self.output = output
        self.ndiff_log = self.in_output('ndiff.log')

        self.pbs_script = self.in_output('pbs_script.qsub')
        self.pbs_output = self.in_output('pbs_output.log')

        self.job_output = self.in_output('job_output.log')
        self.json_output = self.in_output('result.json')
        self.dump_output = self.in_output('result.p')
        self.status_file = self.in_output('runtest.status.json')
        self.valgrind_out = self.in_output('valgrind.out')

        self.input = self.in_root(input)
        self.ref_output = ref_output

    def in_root(self, *names):
        """
        Will return path for file located in root
        :rtype: str
        """
        return Paths.join(self.root, *names)

    def in_output(self, *names):
        """
        Will return path for file located in output
        :rtype: str
        """
        return Paths.join(self.output, *names)