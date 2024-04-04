"""
Author(s): Jan Sobol <sobol@cesnet.cz>

Copyright: (C) 2022 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Base class for probe, generator and collector builder. Used by topology for constructing objects by pytest arguments.
"""

import datetime
import logging
import pkgutil
import tempfile
from abc import ABC, abstractmethod
from os import path
from typing import Union

import lbr_testsuite
import yaml
from lbr_testsuite.executable import AsyncTool, LocalExecutor, RemoteExecutor, Tool
from src.common.utils import get_project_root
from src.config.authentication import AuthenticationCfg
from src.config.collector import CollectorCfg
from src.config.config import Config
from src.config.generator import GeneratorCfg
from src.config.probe import ProbeCfg

ANSIBLE_PATH = path.join(get_project_root(), "ansible")


class BuilderError(Exception):
    """Error while creating object via builder."""


class BuilderBase(ABC):
    """Base class for probe, generator and collector builder. It handles host creation and running ansible playbook."""

    def __init__(self, config: Config, disable_ansible: bool) -> None:
        """Init builder.

        Parameters
        ----------
        config : Config
            Static configuration object.
        disable_ansible: bool
            If True, ansible preparation (with ansible_playbook_role) is disabled.
        """

        self._config = config
        self._executor = None
        self._class = None
        self._timestamp_process = None
        self._disable_ansible = disable_ansible

    @abstractmethod
    def get(self, **kwargs):
        """Create object from static configuration.

        Parameters
        ----------
        kwargs : dict
            Additional arguments passed to object constructor.

        Returns
        -------
        Probe or Generator or Collector
            Object instance.

        Raises
        ------
        NotImplementedError
        """

        raise NotImplementedError

    def get_instance_type(self) -> str:
        """Get type of constructed object.

        Returns
        -------
        str
            Type of instance to build.
        """

        assert self._class is not None
        return self._class.__name__

    def close_connection(self) -> None:
        """Release resources, SSH connection if remote is used.
        Use method in pytest finalizers.
        """

        if self._executor and isinstance(self._executor, RemoteExecutor):
            self._executor.close()

    def host_timestamp_async(self) -> None:
        """Start command for retrieving time on (remote) host."""

        self._timestamp_process = AsyncTool("date +%s%3N", executor=self._executor)
        self._timestamp_process.run()

    def host_timestamp_result(self) -> int:
        """Wait for timestamp command and return timestamp on (remote) host.
        Method 'host_timestamp_async' must be called before.

        Returns
        -------
        int
            Timestamp in ms format (ms since epoch).
        """

        assert self._timestamp_process is not None

        timestamp_str, _ = self._timestamp_process.wait_or_kill()
        timestamp = int(timestamp_str.strip())
        timestamp_datetime = datetime.datetime.fromtimestamp(timestamp / 1000.0, tz=datetime.timezone.utc)

        logging.getLogger().info(
            "Timestamp on host '%s': %d (%s)",
            self._executor.get_host(),
            timestamp,
            timestamp_datetime.isoformat(sep=" ", timespec="milliseconds"),
        )
        return timestamp

    def _prepare_env(self, object_cfg: Union[ProbeCfg, GeneratorCfg, CollectorCfg]) -> None:
        """Connect to host and prepare environment with ansible playbook.

        Parameters
        ----------
        object_cfg : Union[ProbeCfg, GeneratorCfg, CollectorCfg]
            Configuration of object which is to be created.
        """

        auth = self._config.authentications[object_cfg.authentication]
        if object_cfg.name in ["localhost", "127.0.0.1"]:
            self._executor = LocalExecutor()
        else:
            self._executor = RemoteExecutor(object_cfg.name, auth.username, auth.password, auth.key_path)

        if object_cfg.ansible_playbook_role and not self._disable_ansible:
            self._run_ansible(object_cfg.ansible_playbook_role, auth)

    def _run_ansible(self, ansible_playbook_role: str, auth: AuthenticationCfg):
        """Run ansible playbook on REMOTE which required by constructed object. E.g. install probe/generator software.

        Parameters
        ----------
        ansible_playbook_role : str
            Name of ansible playbook.
        auth : AuthenticationCfg
            Object with authentication method, used in ansible inventory.
        """

        user = auth.username if auth.username else lbr_testsuite.get_real_user()
        remote_tmp = f"/tmp/ansible-{user}/tmp"

        host_vars = {"ansible_user": user}
        if auth.password:
            host_vars["ansible_password"] = auth.password
        if auth.key_path:
            host_vars["ansible_ssh_private_key_file"] = auth.key_path
        inventory_data = {"all": {"hosts": {self._executor.get_host(): host_vars}}}

        with tempfile.TemporaryDirectory(prefix="flowtest-ansible-") as local_tmp:
            inventory_path = path.join(local_tmp, "inventory.yaml")
            ssh_control_path_dir = path.join(local_tmp, "cp")

            with open(inventory_path, "w", encoding="utf-8") as inventory_file:
                yaml.safe_dump(inventory_data, inventory_file)

            logging.getLogger().info("Running environment setup - ansible playbook '%s'...", ansible_playbook_role)

            cmd = (
                f"ANSIBLE_SSH_CONTROL_PATH_DIR={ssh_control_path_dir} "
                "ANSIBLE_HOST_KEY_CHECKING=False "
                f"ansible-playbook -i {inventory_path} -e ansible_remote_tmp={remote_tmp} "
                f"{ANSIBLE_PATH}/{ansible_playbook_role}"
            )
            ansible_cmd = Tool(cmd, failure_verbosity="no-exception")
            stdout, stderr = ansible_cmd.run()
            if ansible_cmd.returncode() != 0:
                logging.getLogger().error("Ansible failed!")
                logging.getLogger().error("Stdout: %s", stdout)
                logging.getLogger().error("Stderr: %s", stderr)
                raise BuilderError(f"Ansible playbook '{ansible_playbook_role}' failed.")

        logging.getLogger().info("Ansible output: %s", stdout)

        logging.getLogger().info("Environment setup with ansible done.")

    @staticmethod
    def _find_class(module_paths: list[str], class_name: str, interface: type):
        """Get dynamically imported class by name and interface in given directory.

        Parameters
        ----------
        module_paths : list[str]
            List of paths to directories to search in.
        class_name : str
            Name of searched class.
        interface : type
            Interface which searched class must implement.

        Returns
        -------
        interface type
            Found and imported class.

        Raises
        ------
        BuilderError
            If class not found.
        """

        for finder, submodule_name, is_pkg in pkgutil.walk_packages(module_paths):
            if is_pkg:
                continue
            submodule = finder.find_module(submodule_name).load_module(submodule_name)

            if class_name in dir(submodule):
                searched_class = getattr(submodule, class_name)
                if issubclass(searched_class, interface):
                    return searched_class

        raise BuilderError(f"Class '{class_name}' which implements '{interface.__name__}' not found.")
