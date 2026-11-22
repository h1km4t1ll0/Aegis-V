set -e
test "${BASH_VERSINFO[0]}.${BASH_VERSINFO[1]}" = 4.4
declare -A tools
tools[bash]=4.4
tools[sed]=4.8
tools[bison]=3.0.4
tools[bc]=1.07.1
test "${#tools[@]}" = 4
test "${tools[bison]}" = 3.0.4
[[ ${tools[sed]} =~ ^4\.[0-9]+$ ]]
test "$((6 * 7))" = 42
echo 'bash arrays ok'
