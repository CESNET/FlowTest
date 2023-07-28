#! /usr/bin/env bash

test -z "${ANSIBLE_DIR}" && ANSIBLE_DIR="ansible"
PLAYBOOK="${ANSIBLE_DIR}/dev_machine.yaml"
BECOME="root"
USER=`whoami`
TMP_DIR="/tmp/ansible-${USER}/tmp"
SSH_CONTROL_PATH_DIR="/tmp/ansible-${USER}/cp"
VENV_DIR="venv"

report()
{
	echo "$@" >&2
}

print_help()
{
	report "Usage: ./prepare_machine.sh [-p<playbook>] [-b<user>] [-v] [-h]"
	report "Options:"
	report "  -p<playbook>  - specify path to ansible playbook (default: ${PLAYBOOK})"
	report "  -b<user>      - specify user to become (default: ${BECOME})"
	report "  -r<dir>       - specify tmp dir to be used by ansible (default: ${TMP_DIR})"
	report "  -c<dir>       - specify dir to be used for ssh control path on the local machine (default: ${SSH_CONTROL_PATH_DIR})"
	report "  -s            - skip python virtual environment (pipenv) creation"
	report "  -e<dir>       - change python virtualenv dirname (default: ${VENV_DIR})"
	report "  -v            - increase verbosity of ansible"
	report "  -h            - print help"
}

playbook="${PLAYBOOK}"
become="${BECOME}"
tmp_dir=${TMP_DIR}
ssh_control_path_dir=${SSH_CONTROL_PATH_DIR}
venv_dir=${VENV_DIR}
verbosity=""

while getopts 'hp:b:r:c:se:v' c; do
	case $c in
	h)
		print_help
		exit 0
	;;
	p)
		playbook="$OPTARG"
	;;
	b)
		become="$OPTARG"
	;;
	r)
		tmp_dir="$OPTARG"
	;;
	c)
		ssh_control_path_dir="$OPTARG"
	;;
	s)
		skip_venv=1
	;;
	e)
		venv_dir="$OPTARG"
	;;
	v)
		test -z "${verbosity}" && verbosity="-v" || verbosity="${verbosity}v"
	;;
	*)
		report "Error: Unknown option!"
		exit 1
	;;
	esac
done

if ! command -v ansible-playbook &> /dev/null
then
	report "Error: Ansible must be installed!"
	exit 1
fi

ask_become_pass=""
if ! sudo -vn &> /dev/null
then
	ask_become_pass="--ask-become-pass"
fi

# installation of python3.9 and dev libraries like libpcap
ANSIBLE_SSH_CONTROL_PATH_DIR="${ssh_control_path_dir}" \
ansible-playbook \
	${verbosity} \
	-b --become-user "${become}" ${ask_become_pass} \
	-e ansible_remote_tmp="${tmp_dir}" \
	-i localhost, \
	"${playbook}"

ansible_rc=$?
if [ $ansible_rc -ne 0 ]
then
	report "Error: An error occurred during the execution of the ansible!"
	exit $ansible_rc
fi

# setup virtualenv and install packages
if [ -z "$skip_venv" ]
then
	rm -rf ${venv_dir}
	python3 -m venv ${verbosity} --prompt flowtest ${venv_dir}
	. ${venv_dir}/bin/activate
	pipenv install ${verbosity} --dev

	echo "To activate new python virtualenv run:"
	echo ". ${venv_dir}/bin/activate"
fi
