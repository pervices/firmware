# Updated:  2019-06-06
# Purpose:  Upload the newly archived build files to the FTP server.

#####################
##### VARIABLES #####
#####################

DIRNAME=$1
FILENAME=$2

##################
##### UPLOAD #####
##################

ftp -pinv $FTP_HOST << EOF 1>&2 
user $FTP_USER $FTP_PASS
cd /ci/staging/builds/
lcd $DIRNAME
put $FILENAME
bye
EOF

