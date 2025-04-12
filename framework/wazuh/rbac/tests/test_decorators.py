# Copyright (C) 2015, Wazuh Inc.
# Created by Wazuh, Inc. <info@wazuh.com>.
# This program is a free software; you can redistribute it and/or modify it under the terms of GPLv2

import json
import os
import re
from importlib import reload
from unittest.mock import patch

import pytest
import wazuh.rbac.decorators as decorator
from wazuh.core.exception import WazuhError
from wazuh.core.indexer.models.rbac import Effect, Policy, Rule
from wazuh.core.rbac import RBACManager
from wazuh.core.results import AffectedItemsWazuhResult

test_path = os.path.dirname(os.path.realpath(__file__))
test_data_path = os.path.join(test_path, 'data/')

permissions = list()
results = list()
with open(test_data_path + 'RBAC_decorators_permissions_white.json') as f:
    configurations_white = [
        (
            config['decorator_params'],
            config['function_params'],
            config['rbac'],
            config['fake_system_resources'],
            config['allowed_resources'],
            config.get('result', None),
            'white',
        )
        for config in json.load(f)
    ]
with open(test_data_path + 'RBAC_decorators_permissions_black.json') as f:
    configurations_black = [
        (
            config['decorator_params'],
            config['function_params'],
            config['rbac'],
            config['fake_system_resources'],
            config['allowed_resources'],
            config.get('result', None),
            'black',
        )
        for config in json.load(f)
    ]

with open(test_data_path + 'RBAC_decorators_resourceless_white.json') as f:
    configurations_resourceless_white = [
        (config['decorator_params'], config['rbac'], config['allowed'], 'white') for config in json.load(f)
    ]
with open(test_data_path + 'RBAC_decorators_resourceless_black.json') as f:
    configurations_resourceless_black = [
        (config['decorator_params'], config['rbac'], config['allowed'], 'black') for config in json.load(f)
    ]


def get_identifier(resources):
    """Get resource identifier."""
    list_params = list()
    for resource in resources:
        resource = resource.split('&')
        for r in resource:
            try:
                list_params.append(re.search(r'^([a-z*]+:[a-z*]+:)(\*|{(\w+)})$', r).group(3))
            except AttributeError:
                pass

    return list_params


@pytest.mark.parametrize(
    'decorator_params, function_params, rbac, fake_system_resources, allowed_resources, result, mode',
    configurations_black + configurations_white,
)
async def test_expose_resources(
    decorator_params, function_params, rbac, fake_system_resources, allowed_resources, result, mode
):
    """Validate that the `expose_resources` decorator works as expected."""
    reload(decorator)
    rbac['rbac_mode'] = mode
    decorator.rbac.set(rbac)

    async def mock_expand_resource(resource):
        fake_values = fake_system_resources.get(resource, resource.split(':')[-1])
        return {fake_values} if isinstance(fake_values, str) else set(fake_values)

    with patch('wazuh.rbac.decorators._expand_resource', side_effect=mock_expand_resource):

        @decorator.expose_resources(**decorator_params)
        async def framework_dummy(**kwargs):
            for target_param, allowed_resource in zip(get_identifier(decorator_params['resources']), allowed_resources):
                assert set(kwargs[target_param]) == set(allowed_resource)
                assert 'call_func' not in kwargs
                return True

        try:
            output = await framework_dummy(**function_params)
            assert result is None or result == 'allow'
            assert output == function_params.get('call_func', True) or isinstance(output, AffectedItemsWazuhResult)
        except WazuhError as e:
            assert result is None or result == 'deny'
            for allowed_resource in allowed_resources:
                assert len(allowed_resource) == 0
            assert e.code == 4000


@pytest.mark.parametrize(
    'decorator_params, rbac, allowed, mode', configurations_resourceless_white + configurations_resourceless_black
)
async def test_expose_resourcesless(decorator_params, rbac, allowed, mode):
    """Validate that the `expose_resources` decorator works as expected when no resources are required."""
    reload(decorator)
    rbac['rbac_mode'] = mode
    decorator.rbac.set(rbac)

    async def mock_expand_resource(resource):
        return {'*'}

    with patch('wazuh.rbac.decorators._expand_resource', side_effect=mock_expand_resource):

        @decorator.expose_resources(**decorator_params)
        async def framework_dummy():
            pass

        try:
            await framework_dummy()
            assert allowed
        except WazuhError as e:
            assert not allowed
            assert e.code == 4000


async def test__expand_resource():
    """Validate that the `_expand_resource` function works as expected."""
    reload(decorator)
    rbac_manager = RBACManager()
    rbac_manager._rules = {
        'rule1': Rule(
            name='rule1',
            body={'FIND': {"r''^auth[a-zA-Z]+$''": ['administrator']}},
        ),
        'rule2': Rule(
            name='rule2',
            body={'FIND': {"r''^auth[a-zA-Z]+$''": ['administrator-app']}},
        ),
        'rule3': Rule(
            name='rule3',
            body={'MATCH': {'definition': 'technicalRule'}},
        ),
    }
    decorator.rbac_manager.set(rbac_manager)

    result = await decorator._expand_resource('rule:id:*')
    assert {rule.name for rule in rbac_manager.get_rules()} == result


def test_get_rbac_manager():
    """Validate that the `get_rbac_manager` function works as expected."""
    policy_name = 'test'
    rbac_manager = RBACManager()
    rbac_manager._policies = {
        policy_name: Policy(
            name=policy_name,
            actions=['group:create'],
            resources=['*:*:*'],
            effect=Effect.ALLOW,
            level=0,
        ),
    }
    decorator.rbac_manager.set(rbac_manager)

    with decorator.get_rbac_manager() as rbac_manager:
        policy = rbac_manager.get_policy(policy_name)

    assert policy == rbac_manager._policies[policy_name]
