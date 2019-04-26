#
# @mindmaze_header@
#
# This script allows you to see whether and which prefix is mounted in your
# shell process.
#
# To enable:
#
#    1) Add the following line to your .bashrc:
#         source <PKGDATADIR>/prompt.bash
#    2) Modify your PS1 or PROMPT_COMMAND definition to include
#       $mmpack_ps1_elt where you see fit.
#
# Example:
#   source <PKGDATADIR>/prompt.bash
#   PS1='$mmpack_ps1_elt\u@\h:\w\$ '
#
# The returned mmpack_ps1_elt variable will be empty if no prefix is
# currently mounted. Otherwise, it is expanded according a format which is
# by default '(%s)' where %s is substituted by prefix name.
#
# Before sourcing this file you may define two variables which control the
# resulting mmpack_ps1_elt variable:
#   - mmpack_ps1_elt_fmt: set the format of expansion instead of '(%s)'
#   - color_prompt: if non empty, the prefix name will be colorized


# This function set mmpack_ps1_elt shell variable. If an argument is
# provided, it will be used to specify the expansion format instead of the
# default '(%s)'
__mmpack_ps1_elt ()
{
	local ps1_elt_fmt=${1:-'(%s)'}
	local name=$(basename $MMPACK_ACTIVE_PREFIX)
	local mmpack_string=""
	local c_red=$(tput setaf 124)
	local c_clear=$(tput sgr0)

	if [ -n "$color_prompt" ]; then
		mmpack_string="$c_red$name$c_clear"
	else
		mmpack_string=$name
	fi

	printf -v mmpack_ps1_elt "$ps1_elt_fmt" "$mmpack_string"
}


mmpack_ps1_elt=""
if [ -n "$MMPACK_ACTIVE_PREFIX" ]; then
	__mmpack_ps1_elt $mmpack_ps1_elt_fmt
fi
