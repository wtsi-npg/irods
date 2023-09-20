from __future__ import print_function

import copy
import os
import unittest

from . import session
from .. import lib
from .. import test
from ..configuration import IrodsConfig
from ..controller import IrodsController
from ..core_file import temporary_core_file


@unittest.skipIf(test.settings.USE_SSL, 'SSL is set up in these tests, so just skip if SSL is enabled already.')
class test_configurations(unittest.TestCase):
    plugin_name = IrodsConfig().default_rule_engine_plugin

    @staticmethod
    def generate_default_ssl_files(ssl_directory=None, numbits_for_genrsa=2048):
        ssl_directory = ssl_directory or os.path.join(IrodsConfig().irods_directory, 'test')

        server_key_path = os.path.join(ssl_directory, 'server.key')
        chain_pem_path = os.path.join(ssl_directory, 'chain.pem')
        dhparams_pem_path = os.path.join(ssl_directory, 'dhparams.pem')

        lib.execute_command(['openssl', 'genrsa', '-out', server_key_path, str(numbits_for_genrsa)])
        lib.execute_command(
            ['openssl', 'req', '-batch', '-new', '-x509', '-key', server_key_path, '-out', chain_pem_path, '-days', '365'])
        lib.execute_command(['openssl', 'dhparam', '-2', '-out', dhparams_pem_path, str(numbits_for_genrsa)])

        return (server_key_path, chain_pem_path, dhparams_pem_path)

    @staticmethod
    def make_dict_for_ssl_client_environment(server_key_path, chain_pem_path, dhparams_pem_path, ca_certificate_path=None):
        ca_certificate_path = ca_certificate_path or chain_pem_path
        return {
            'irods_client_server_negotiation': 'request_server_negotiation',
            'irods_client_server_policy': 'CS_NEG_REQUIRE',
            'irods_ssl_certificate_chain_file': chain_pem_path,
            'irods_ssl_certificate_key_file': server_key_path,
            'irods_ssl_dh_params_file': dhparams_pem_path,
            'irods_ssl_ca_certificate_file': ca_certificate_path,
            'irods_ssl_verify_server': 'none'
        }

    @staticmethod
    def get_pep_for_ssl(plugin_name):
        import textwrap

        return {
            'irods_rule_engine_plugin-irods_rule_language': textwrap.dedent('''
                acPreConnect(*OUT) {
                    *OUT = 'CS_NEG_REQUIRE';
                }
            '''),
            'irods_rule_engine_plugin-python': textwrap.dedent('''
                def acPreConnect(rule_args, callback, rei):
                    rule_args[0] = 'CS_NEG_REQUIRE'
            ''')
        }[plugin_name]

    @classmethod
    def setUpClass(self):
        self.admin = session.mkuser_and_return_session('rodsadmin', 'otherrods', 'rods', lib.get_hostname())

        cfg = lib.open_and_load_json(
            os.path.join(IrodsConfig().irods_directory, 'test', 'test_framework_configuration.json'))
        auth_user = cfg['irods_authuser_name']
        auth_pass = cfg['irods_authuser_password']

        # Requires existence of OS account 'irodsauthuser' with password ';=iamnotasecret'
        try:
            import pwd
            pwd.getpwnam(auth_user)

        except KeyError:
            # This is a requirement in order to run these tests and running the tests is required for our test suite, so
            # we always fail here when the prerequisites are not being met on the test-running host.
            self.fail('OS user "[{}]" with password "[{}]" must exist in order to run these tests.'.format(
                auth_user, auth_pass))

        self.auth_session = session.mkuser_and_return_session('rodsuser', auth_user, auth_pass, lib.get_hostname())
        self.service_account_environment_file_path = os.path.join(
            os.path.expanduser('~'), '.irods', 'irods_environment.json')

        self.server_key_path, self.chain_pem_path, self.dhparams_pem_path = test_configurations.generate_default_ssl_files()
        self.authentication_scheme = 'pam_password'
        self.configuration_namespace = 'authentication::pam_password'

    @classmethod
    def tearDownClass(self):
        self.auth_session.__exit__()

        for filename in [self.chain_pem_path, self.server_key_path, self.dhparams_pem_path]:
            if os.path.exists(filename):
                os.unlink(filename)

        self.admin.assert_icommand(['iadmin', 'rmuser', self.auth_session.username])
        self.admin.__exit__()
        with session.make_session_for_existing_admin() as admin_session:
            admin_session.assert_icommand(['iadmin', 'rmuser', self.admin.username])

    def do_test_invalid_password_time_configurations(self, _option_name):
        # Stash away the original configuration for later...
        original_config = self.admin.assert_icommand(
                ['iadmin', 'get_grid_configuration', self.configuration_namespace, _option_name], 'STDOUT')[1].strip()

        IrodsController().stop()

        auth_session_env_backup = copy.deepcopy(self.auth_session.environment_file_contents)
        admin_session_env_backup = copy.deepcopy(self.admin.environment_file_contents)
        try:
            service_account_environment_file_path = os.path.join(
                os.path.expanduser('~'), '.irods', 'irods_environment.json')
            with lib.file_backed_up(service_account_environment_file_path):
                client_update = test_configurations.make_dict_for_ssl_client_environment(
                    self.server_key_path, self.chain_pem_path, self.dhparams_pem_path)

                lib.update_json_file_from_dict(service_account_environment_file_path, client_update)

                self.admin.environment_file_contents.update(client_update)

                client_update['irods_authentication_scheme'] = self.authentication_scheme
                self.auth_session.environment_file_contents.update(client_update)

                with temporary_core_file() as core:
                    core.add_rule(test_configurations.get_pep_for_ssl(self.plugin_name))

                    IrodsController().start()

                    # Make sure the settings applied correctly...
                    self.admin.assert_icommand(
                        ['iadmin', 'get_grid_configuration', self.configuration_namespace, _option_name],
                        'STDOUT', original_config)

                    self.auth_session.assert_icommand(
                        ['iinit'], 'STDOUT', 'PAM password', input=f'{self.auth_session.password}\n')
                    self.auth_session.assert_icommand(["ils"], 'STDOUT', self.auth_session.session_collection)

                    for option_value in [' ', 'nope', str(-1), str(18446744073709552000), str(-18446744073709552000)]:
                        with self.subTest(f'test with value [{option_value}]'):
                            self.admin.assert_icommand(
                                ['iadmin', 'set_grid_configuration', '--', self.configuration_namespace, _option_name, option_value])

                            # These invalid configurations will not cause any errors, but default values will be used.
                            self.auth_session.assert_icommand(
                                ['iinit'], 'STDOUT', 'PAM password', input=f'{self.auth_session.password}\n')
                            self.auth_session.assert_icommand(["ils"], 'STDOUT', self.auth_session.session_collection)

        finally:
            self.auth_session.environment_file_contents = auth_session_env_backup
            self.admin.environment_file_contents = admin_session_env_backup

            IrodsController().restart()

            self.admin.assert_icommand(
                ['iadmin', 'set_grid_configuration', self.configuration_namespace, _option_name, original_config])

    def test_invalid_password_max_time(self):
        self.do_test_invalid_password_time_configurations('password_max_time')

    def test_invalid_password_min_time(self):
        self.do_test_invalid_password_time_configurations('password_min_time')

    def test_password_max_time_less_than_password_min_time_makes_ttl_constraints_unsatisfiable(self):
        min_time_option_name = 'password_min_time'
        max_time_option_name = 'password_max_time'

        # Stash away the original configuration for later...
        original_min_time = self.admin.assert_icommand(
            ['iadmin', 'get_grid_configuration', self.configuration_namespace, min_time_option_name], 'STDOUT')[1].strip()

        original_max_time = self.admin.assert_icommand(
            ['iadmin', 'get_grid_configuration', self.configuration_namespace, max_time_option_name], 'STDOUT')[1].strip()

        IrodsController().stop()

        auth_session_env_backup = copy.deepcopy(self.auth_session.environment_file_contents)
        admin_session_env_backup = copy.deepcopy(self.admin.environment_file_contents)
        try:
            service_account_environment_file_path = os.path.join(
                os.path.expanduser('~'), '.irods', 'irods_environment.json')
            with lib.file_backed_up(service_account_environment_file_path):
                client_update = test_configurations.make_dict_for_ssl_client_environment(
                    self.server_key_path, self.chain_pem_path, self.dhparams_pem_path)

                lib.update_json_file_from_dict(service_account_environment_file_path, client_update)

                self.admin.environment_file_contents.update(client_update)

                client_update['irods_authentication_scheme'] = self.authentication_scheme
                self.auth_session.environment_file_contents.update(client_update)

                with temporary_core_file() as core:
                    core.add_rule(test_configurations.get_pep_for_ssl(self.plugin_name))

                    IrodsController().start()

                    # Make sure the settings applied correctly...
                    self.admin.assert_icommand(
                        ['iadmin', 'get_grid_configuration', self.configuration_namespace, max_time_option_name],
                        'STDOUT', original_max_time)

                    # Try a few different values here that are in the range of overall acceptable values:
                    #     - 2 hours allows us to go up OR down by 1 hour (boundary case).
                    #     - 336 hours is 1209600 seconds (or 2 weeks) which is the default maximum allowed TTL value.
                    for base_ttl_in_hours in [2, 336]:
                        with self.subTest(f'test with TTL of [{base_ttl_in_hours}] hours'):
                            base_ttl_in_seconds = base_ttl_in_hours * 3600

                            option_value = str(base_ttl_in_seconds + 10)
                            self.admin.assert_icommand(
                                ['iadmin', 'set_grid_configuration', self.configuration_namespace, min_time_option_name, option_value])

                            # Set password_max_time to a value less than the password_min_time.
                            option_value = str(base_ttl_in_seconds - 10)
                            self.admin.assert_icommand(
                                ['iadmin', 'set_grid_configuration', self.configuration_namespace, max_time_option_name, option_value])

                            # Note: The min/max check does not occur when no TTL parameter is passed, for some reason.
                            # We must pass TTL explicitly for each test.

                            # This is lower than the minimum and higher than the maximum. The TTL is invalid.
                            self.auth_session.assert_icommand(
                                ['iinit', '--ttl', str(base_ttl_in_hours)],
                                 'STDERR', 'PAM_AUTH_PASSWORD_INVALID_TTL', input=f'{self.auth_session.password}\n')

                            # This is lower than the maximum but also lower than the minimum. The TTL is invalid.
                            self.auth_session.assert_icommand(
                                ['iinit', '--ttl', str(base_ttl_in_hours - 1)],
                                 'STDERR', 'PAM_AUTH_PASSWORD_INVALID_TTL', input=f'{self.auth_session.password}\n')

                            # This is higher than the minimum but also higher than the maximum. The TTL is invalid.
                            self.auth_session.assert_icommand(
                                ['iinit', '--ttl', str(base_ttl_in_hours + 1)],
                                 'STDERR', 'PAM_AUTH_PASSWORD_INVALID_TTL', input=f'{self.auth_session.password}\n')

                    # Restore grid configuration and try again, with success.
                    self.admin.assert_icommand(
                        ['iadmin', 'set_grid_configuration', self.configuration_namespace, max_time_option_name, original_max_time])
                    self.admin.assert_icommand(
                        ['iadmin', 'set_grid_configuration', self.configuration_namespace, min_time_option_name, original_min_time])

                    self.auth_session.assert_icommand(
                        ['iinit', '--ttl', str(1)], 'STDOUT', input=f'{self.auth_session.password}\n')
                    self.auth_session.assert_icommand(["ils"], 'STDOUT', self.auth_session.session_collection)

        finally:
            self.auth_session.environment_file_contents = auth_session_env_backup
            self.admin.environment_file_contents = admin_session_env_backup

            IrodsController().restart()

            self.admin.assert_icommand(
                ['iadmin', 'set_grid_configuration', self.configuration_namespace, max_time_option_name, original_max_time])
            self.admin.assert_icommand(
                ['iadmin', 'set_grid_configuration', self.configuration_namespace, min_time_option_name, original_min_time])

            IrodsController().restart()

    def test_password_expires_appropriately_based_on_grid_configuration_value(self):
        import time

        min_time_option_name = 'password_min_time'
        max_time_option_name = 'password_max_time'

        # Stash away the original configuration for later...
        original_min_time = self.admin.assert_icommand(
            ['iadmin', 'get_grid_configuration', self.configuration_namespace, min_time_option_name], 'STDOUT')[1].strip()

        original_max_time = self.admin.assert_icommand(
            ['iadmin', 'get_grid_configuration', self.configuration_namespace, max_time_option_name], 'STDOUT')[1].strip()

        IrodsController().stop()

        auth_session_env_backup = copy.deepcopy(self.auth_session.environment_file_contents)
        admin_session_env_backup = copy.deepcopy(self.admin.environment_file_contents)
        try:
            service_account_environment_file_path = os.path.join(
                os.path.expanduser('~'), '.irods', 'irods_environment.json')
            with lib.file_backed_up(service_account_environment_file_path):
                client_update = test_configurations.make_dict_for_ssl_client_environment(
                    self.server_key_path, self.chain_pem_path, self.dhparams_pem_path)

                lib.update_json_file_from_dict(service_account_environment_file_path, client_update)

                self.admin.environment_file_contents.update(client_update)

                client_update['irods_authentication_scheme'] = self.authentication_scheme
                self.auth_session.environment_file_contents.update(client_update)

                with temporary_core_file() as core:
                    core.add_rule(test_configurations.get_pep_for_ssl(self.plugin_name))

                    IrodsController().start()

                    # Make sure the settings applied correctly...
                    self.admin.assert_icommand(
                        ['iadmin', 'get_grid_configuration', self.configuration_namespace, max_time_option_name],
                        'STDOUT', original_max_time)

                    # When no TTL is specified, the default value is the minimum password lifetime as configured in
                    # R_GRID_CONFIGURATION.
                    ttl = 4
                    option_value = str(ttl)
                    self.admin.assert_icommand(
                        ['iadmin', 'set_grid_configuration', self.configuration_namespace, min_time_option_name, option_value])

                    # Authenticate and run a command...
                    self.auth_session.assert_icommand(
                        ['iinit'], 'STDOUT', 'PAM password', input=f'{self.auth_session.password}\n')

                    self.auth_session.assert_icommand(["ils"], 'STDOUT', self.auth_session.session_collection)

                    # Sleep until the password is expired...
                    time.sleep(ttl + 1)

                    # Password should be expired now...
                    self.auth_session.assert_icommand(["ils"], 'STDERR', 'CAT_PASSWORD_EXPIRED: failed to perform request')

                    # ...and stays expired.
                    # TODO: irods/irods#7344 - This should emit a better error message.
                    self.auth_session.assert_icommand(["ils"], 'STDERR', 'CAT_INVALID_AUTHENTICATION: failed to perform request')

                    # Restore grid configuration and try again, with success.
                    self.admin.assert_icommand(
                        ['iadmin', 'set_grid_configuration', self.configuration_namespace, max_time_option_name, original_max_time])
                    self.admin.assert_icommand(
                        ['iadmin', 'set_grid_configuration', self.configuration_namespace, min_time_option_name, original_min_time])

                    self.auth_session.assert_icommand(['iinit'], 'STDOUT', input=f'{self.auth_session.password}\n')
                    self.auth_session.assert_icommand(["ils"], 'STDOUT', self.auth_session.session_collection)

        finally:
            self.auth_session.environment_file_contents = auth_session_env_backup
            self.admin.environment_file_contents = admin_session_env_backup

            IrodsController().restart()

            self.admin.assert_icommand(
                ['iadmin', 'set_grid_configuration', self.configuration_namespace, max_time_option_name, original_max_time])
            self.admin.assert_icommand(
                ['iadmin', 'set_grid_configuration', self.configuration_namespace, min_time_option_name, original_min_time])

            IrodsController().restart()
