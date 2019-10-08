# Custom script to generate src/version.h file containing the git commit hash
#
# See also http://docs.platformio.org/en/latest/projectconf.html#extra-script
#

import subprocess

try:
    commit_hash = subprocess.check_output(["git", "rev-parse", "HEAD"]).strip()
except:
    commit_hash = ""

f = open("src/version.h","w+")
f.write("/* Auto-generated file with git commit hash to be included by C code\n")
f.write(" * Do not change this file manually!\n")
f.write(" */\n")
f.write("\n")
f.write('#define COMMIT_HASH "' + commit_hash.decode('utf-8') + '"\n')
f.close
