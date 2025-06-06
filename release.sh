#!/bin/bash

echo "This script will send the tarballs to infradead and create tags and release at gitlab"
echo "It will use your ssh keys and gitlab token as placed in ~/.gitlab-token"
echo "Press enter to continue..."
read

if test -z "$1";then
	echo "usage: $0 [VERSION]"
	echo "No version was specified"
	exit 1
fi

if ! test -f "$(expr ~/.gitlab-token)";then
	echo "Cannot find .gitlab-token"
	exit 1
fi

PROJECT=473862
TOKEN=$(cat ~/.gitlab-token)
version=$1
file=ocserv-${version}.tar.xz

echo "Signing artifacts as generated by 'make dist'"
echo "Press enter to continue or type skip to skip..."
read s

if test "$s" != "s" && test "$s" != "skip";then
	gpg --sign --detach ${file}
	scp ${file}* casper.infradead.org:/var/ftp/pub/ocserv/
fi

echo "Creating tag $version and gitlab release"
echo "Press enter to continue or type skip to skip..."
read s
if test "$s" != "s" && test "$s" != "skip";then
	git tag -s ${version} -m "Released ${version}"
	git push origin ${version}
fi

echo ""
echo "Checking for $version milestone"
echo "Press enter to continue or type skip to skip..."
read s

milestones=''
if test "$s" != "s" && test "$s" != "skip";then
	curl -s --header "PRIVATE-TOKEN: ${TOKEN}" "https://gitlab.com/api/v4/projects/${PROJECT}/milestones?title=$version"|grep "$version" >/dev/null 2>&1
	if [[ $? = 0 ]];then
		echo "Milestone $version will be linked to release"
		milestones=' "milestones": [ "'${version}'" ],'
	else
		echo "No matching milestones"
		milestones=''
	fi
fi

echo "Creating gitlab $version release"
echo "Press enter to continue or type skip to skip..."
read s

if test "$s" != "s" && test "$s" != "skip";then
	line=$(grep -n "Version ${version}" NEWS|cut -d ':' -f 1)
	test -z "$line" && exit 1

	stopline="$(head -n 100 NEWS|tail -n $((100-$line))|grep -n Version|head -1|cut -d ':' -f 1)"
	test -z "$stopline" && exit 1

	msg=$(head -n 100 NEWS|tail -n +$((1+$line))|head -n $(($stopline-1))|tr -d '"'|tr -d "'"|sed '{:q;N;s/\n/\\n/g;t q}')

	set -e
	curl -s --header 'Content-Type: application/json' --header "PRIVATE-TOKEN: ${TOKEN}" \
	     --data '{ "name": "'${version}'", "tag_name": "'${version}'", "description": "'"${msg}"'", '"${milestones}"' "assets": { "links": [{ "name": "PGP signature", "url": "https://www.infradead.org/ocserv/download/ocserv-'${version}'.tar.xz.sig", "link_type":"other" }, { "name": "Tarball", "url": "https://www.infradead.org/ocserv/download/ocserv-'${version}'.tar.xz", "link_type":"package" }] } }' \
	     --request POST "https://gitlab.com/api/v4/projects/${PROJECT}/releases" | jq
fi

echo ""
echo "release is ready: https://gitlab.com/openconnect/ocserv/-/releases"

exit 0
