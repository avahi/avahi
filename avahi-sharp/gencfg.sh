#!/bin/bash

. $1
client_dlname=$dlname
. $2
common_dlname=$dlname

exec sed -e "s,@CLIENT_DLNAME\@,${client_dlname},g" \
         -e "s,@COMMON_DLNAME\@,${common_dlname},g"
