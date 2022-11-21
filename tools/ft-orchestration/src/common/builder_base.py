"""
Author(s): Jan Sobol <sobol@cesnet.cz>

Copyright: (C) 2022 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Base class for probe, generator and collector builder. Used by topology for constructing objects by pytest arguments.
"""


import logging
import os
import pkgutil
import subprocess
from abc import ABC, abstractmethod
from os import path
from typing import Union

import yaml
from src.config.authentication import AuthenticationCfg
from src.config.collector import CollectorCfg
from src.config.config import Config
from src.config.generator import GeneratorCfg
from src.config.probe import ProbeCfg
from src.host.common import get_real_user
from src.host.host import Host
from src.host.storage import RemoteStorage

ANSIBLE_PATH = path.join(path.dirname(path.realpath(__file__)), "../../../../ansible")


class BuilderError(Exception):
    """Error while creating object via builder."""


class BuilderBase(ABC):
    """Base class for probe, generator and collector builder. It handles host creation and running ansible playbook."""

    def __init__(self, config: Config) -> None:
        """Init builder.

        Parameters
        ----------
        config : Config
            Static configuration object.
        """

        self._config = config
        self._host = None
        self._storage = None
        self._class = None

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

    def get_instance_type(self) -> type:
        """Get type of constructed object.

        Returns
        -------
        type
            Type of instance to build.
        """

        assert self._class is not None
        return self._class

    def close_connection(self) -> None:
        """Release resources, SSH connection if remote is used.
        Use method in pytest finalizers.
        """

        if self._host:
            self._host.close()
        if self._storage:
            self._storage.close()

    def _prepare_env(self, object_cfg: Union[ProbeCfg, GeneratorCfg, CollectorCfg]) -> None:
        """Connect to host and prepare environment with ansible playbook.

        Parameters
        ----------
        object_cfg : Union[ProbeCfg, GeneratorCfg, CollectorCfg]
            Configuration of object which is to be created.
        """

        auth = self._config.authentications[object_cfg.authentication]
        self._storage = (
            RemoteStorage(object_cfg.name, None, auth.username, auth.password, auth.key_path)
            if object_cfg.name != "localhost"
            else None
        )
        self._host = Host(object_cfg.name, self._storage, auth.username, auth.password, auth.key_path)

        if object_cfg.ansible_playbook_role:
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

        user = auth.username if auth.username else get_real_user()
        local_tmp = f"/tmp/flowtest-ansible-{user}"
        inventory_path = path.join(local_tmp, "inventory.yaml")
        remote_tmp = f"/tmp/ansible-{user}/tmp"
        ssh_control_path_dir = f"/tmp/ansible-{user}/cp"

        host_vars = {"ansible_user": user}
        if auth.password:
            host_vars["ansible_password"] = auth.password
        if auth.key_path:
            host_vars["ansible_ssh_private_key_file"] = auth.key_path
        inventory_data = {"all": {"hosts": {self._host.get_host(): host_vars}}}

        os.makedirs(local_tmp, exist_ok=True)
        with open(inventory_path, "w", encoding="utf-8") as inventory_file:
            yaml.safe_dump(inventory_data, inventory_file)

        logging.getLogger().info("Running environment setup - ansible playbook '%s'...", ansible_playbook_role)

        cmd = (
            f"ANSIBLE_SSH_CONTROL_PATH_DIR={ssh_control_path_dir} "
            "ANSIBLE_HOST_KEY_CHECKING=False "
            f"ansible-playbook -i {inventory_path} -e ansible_remote_tmp={remote_tmp} "
            f"{ANSIBLE_PATH}/{ansible_playbook_role}"
        )
        # temporary solution, use LocalExecutor after implementation in testsuite
        res = subprocess.run(cmd, check=True, shell=True, capture_output=True)
        logging.getLogger().info("Ansible output: %s", res.stdout.decode("ascii"))

        logging.getLogger().info("Environment setup with ansible done.")

    @staticmethod
    def _find_class(module_path: str, class_name: str, interface: type):
        """Get dynamically imported class by name and interface in given directory.

        Parameters
        ----------
        module_path : str
            Path to a directory to search in.
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

        for finder, submodule_name, is_pkg in pkgutil.walk_packages([module_path]):
            if is_pkg:
                continue
            submodule = finder.find_module(submodule_name).load_module(submodule_name)

            if class_name in dir(submodule):
                searched_class = getattr(submodule, class_name)
                if issubclass(searched_class, interface):
                    return searched_class

        raise BuilderError(f"Class '{class_name}' which implements '{interface.__name__}' not found.")
