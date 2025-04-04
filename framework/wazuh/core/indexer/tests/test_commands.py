from dataclasses import asdict
from unittest import mock

import pytest
from opensearchpy import exceptions
from opensearchpy.helpers.response import Hit
from wazuh.core.exception import WazuhError
from wazuh.core.indexer.base import POST_METHOD, IndexerKey
from wazuh.core.indexer.commands import (
    COMMAND_KEY,
    COMMAND_USER_NAME,
    CommandsManager,
    create_fetch_config_command,
    create_restart_command,
    create_set_group_command,
)
from wazuh.core.indexer.models.commands import (
    Action,
    Command,
    CreateCommandResponse,
    ResponseResult,
    Source,
    Status,
    Target,
    TargetType,
)
from wazuh.core.indexer.utils import convert_enums


class TestCommandsManager:
    """Class to validate that the commands manager methods behave as expected."""

    index_class = CommandsManager
    create_command = Command(source=Source.ENGINE)

    @pytest.fixture
    def client_mock(self) -> mock.AsyncMock:
        """Indexer client mock.

        Returns
        -------
        mock.AsyncMock
            Client mock.
        """
        return mock.AsyncMock()

    @pytest.fixture
    def manager_instance(self, client_mock: mock.AsyncMock) -> CommandsManager:
        """Commands manager mock.

        Parameters
        ----------
        client_mock : mock.AsyncMock
            Indexer client mock.

        Returns
        -------
        CommandsManager
            Commands manager instance.
        """
        return self.index_class(client=client_mock)

    async def test_create(self, manager_instance: CommandsManager, client_mock: mock.AsyncMock):
        """Check the correct functionality of the `create` method."""
        return_value = {
            IndexerKey._INDEX: CommandsManager.INDEX,
            IndexerKey._DOCUMENTS: [
                {IndexerKey._ID: 'pBjePGfvgm'},
                {IndexerKey._ID: 'mp2Xymz6F3'},
                {IndexerKey._ID: 'aYsJYUEmVk'},
                {IndexerKey._ID: 'QTjrFfpoIS'},
            ],
            IndexerKey.RESULT: ResponseResult.CREATED,
        }
        client_mock.transport.perform_request.return_value = return_value

        response = await manager_instance.create([self.create_command])

        assert isinstance(response, CreateCommandResponse)
        client_mock.transport.perform_request.assert_called_once_with(
            method=POST_METHOD,
            url=f'{manager_instance.PLUGIN_URL}/commands',
            body={
                'commands': [
                    asdict(self.create_command, dict_factory=convert_enums),
                ]
            },
        )

        document_ids = [document.get(IndexerKey._ID) for document in return_value.get(IndexerKey._DOCUMENTS)]
        assert response.index == return_value.get(IndexerKey._INDEX)
        assert response.document_ids == document_ids
        assert response.result == return_value.get(IndexerKey.RESULT)

    @pytest.mark.parametrize('exc', [exceptions.RequestError, exceptions.TransportError])
    async def test_create_ko(self, manager_instance: CommandsManager, client_mock: mock.AsyncMock, exc):
        """Check the error handling of the `create` method."""
        client_mock.transport.perform_request.side_effect = exc(400, 'error')
        with pytest.raises(WazuhError, match='.*1761.*'):
            await manager_instance.create([self.create_command])

    async def test_get_commands(self, manager_instance: CommandsManager, client_mock: mock.AsyncMock):
        """Check the correct functionality of the `get_commands` method."""
        document_id = '0191c248-095c-75e6-89ec-612fa5727c2e'
        search_result = {
            '_hits': [
                Hit({IndexerKey._ID: document_id, IndexerKey._SOURCE: {COMMAND_KEY: {'source': Source.SERVICES}}})
            ]
        }
        client_mock.search.return_value = search_result
        expected_result = [Command(document_id=document_id, source=Source.SERVICES)]

        result = await manager_instance.get_commands(Status.PENDING)

        query = {
            IndexerKey.QUERY: {
                IndexerKey.BOOL: {IndexerKey.FILTER: [{IndexerKey.TERM: {'command.status': Status.PENDING}}]}
            }
        }
        client_mock.search.assert_called_once_with(index=[manager_instance.INDEX], body=query)

        assert result == expected_result

    @pytest.mark.parametrize('exc', [exceptions.RequestError, exceptions.TransportError])
    async def test_get_commands_ko(self, manager_instance: CommandsManager, client_mock: mock.AsyncMock, exc):
        """Check that the `get_commands` method handles exceptions successfully."""
        client_mock.search.side_effect = exc(400, 'error')

        with pytest.raises(WazuhError, match=r'1761'):
            await manager_instance.get_commands(Status.PENDING)

    async def test_update_commands_status(self, manager_instance: CommandsManager, client_mock: mock.AsyncMock):
        """Check the correct function of `update_commands_status` method."""
        order_ids = ['123', '456']
        status = 'foo'
        await manager_instance.update_commands_status(order_ids=order_ids, status=status)

        query = {
            IndexerKey.QUERY: {
                IndexerKey.BOOL: {IndexerKey.FILTER: [{IndexerKey.TERMS: {'command.order_id': order_ids}}]}
            },
            'script': {
                'source': CommandsManager.UPDATE_STATUS_SCRIPT,
                'lang': 'painless',
                'params': {'status': status},
            },
        }
        client_mock.update_by_query.assert_called_once_with(index=[manager_instance.INDEX], body=query)


def test_create_restart_command():
    """Check the correct functionality of the `create_restart_command` function."""
    agent_id = '0191dd54-bd16-7025-80e6-ae49bc101c7a'
    expected_command = Command(
        source=Source.SERVICES,
        target=Target(
            type=TargetType.AGENT,
            id=agent_id,
        ),
        action=Action(name='restart', version='5.0.0'),
        user=COMMAND_USER_NAME,
        timeout=100,
    )

    command = create_restart_command(agent_id=agent_id)

    assert command == expected_command
    assert command.document_id is None


def test_create_set_group_command():
    """Check the correct functionality of the `create_set_group_command` function."""
    agent_id = '0191dd54-bd16-7025-80e6-ae49bc101c7a'
    groups = ['default', 'group1', 'group3']
    expected_command = Command(
        source=Source.SERVICES,
        target=Target(
            type=TargetType.AGENT,
            id=agent_id,
        ),
        action=Action(name='set-group', args={'groups': groups}, version='5.0.0'),
        user=COMMAND_USER_NAME,
        timeout=100,
    )

    command = create_set_group_command(agent_id=agent_id, groups=groups)

    assert command == expected_command
    assert command.document_id is None


def test_create_fetch_config_command():
    """Check the correct functionality of the `create_fetch_config_command` function."""
    agent_id = '0191dd54-bd16-7025-80e6-ae49bc101c7a'
    expected_command = Command(
        source=Source.SERVICES,
        target=Target(
            type=TargetType.AGENT,
            id=agent_id,
        ),
        action=Action(name='fetch-config', version='5.0.0', args={}),
        user=COMMAND_USER_NAME,
        timeout=100,
    )

    command = create_fetch_config_command(agent_id=agent_id)

    assert command == expected_command
    assert command.document_id is None
