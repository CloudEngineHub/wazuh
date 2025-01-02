#!/usr/bin/env python3
from health_test.test_suite import UnitResultInterface, UnitOutput, run as suite_run


class UnitResult(UnitResultInterface):
    def __init__(self, index: int, expected: dict, actual: UnitOutput, target: str, help: str):
        self.index = index
        self.expected = expected
        if not actual.success:
            self.error = actual.error
            self.success = False
        else:
            self.setup(actual.output)

    def setup(self, actual: dict):
        self.diff = {}
        filtered_expected = {
            k: (v if k != "event" else {sub_k: sub_v for sub_k, sub_v in v.items() if sub_k != "start"}) for k,
            v in self.expected.items() if k != "@timestamp"}
        filtered_actual = {k: (v if k != "event" else {sub_k: sub_v for sub_k, sub_v in v.items() if sub_k != "start"})
                           for k, v in actual.items() if k != "@timestamp"}

        if filtered_expected == filtered_actual:
            self.success = True
            return
        else:
            self.success = False

        for key in filtered_expected:
            if key not in filtered_actual:
                self.diff[key] = {"info": "Missing key in actual result",
                                  "expected": filtered_expected[key]}
                return
            elif filtered_expected[key] != filtered_actual[key]:
                self.diff[key] = {"info": "Mismatched value",
                                  "expected": filtered_expected[key], "actual": filtered_actual[key]}
        for key in filtered_actual:
            if key not in filtered_expected:
                self.diff[key] = {"info": "Extra key in actual result",
                                  "actual": filtered_actual[key]}


def run(args):
    return suite_run(args, UnitResult, debug_mode="")
