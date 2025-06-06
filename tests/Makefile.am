include ../src/common.mk

EXTRA_DIST = certs/ca-key.pem certs/ca.pem ns.sh common.sh certs/server-cert.pem \
	certs/server-key.pem data/test1.config data/pam/nss-group.in data/pam/nss-passwd.in \
	data/pam/users.oath.templ data/test-pam-noauth.config data/test-pam.passwd \
	data/test1.passwd data/test-user-cert.config certs/user-cert.pem certs/user-key.pem \
	data/test3.config data/test-iroute.config data/test-pam.config data/test-vhost2.passwd \
	user-config/test user-config-opt/test data/test-pass-script.config data/test-multi-cookie.config \
	data/test-stress.config certs/user-cert-wrong.pem connect-script data/test-group.passwd \
	data/test-group-pass.config certs/user-group-cert.pem certs/user-group-key.pem \
	data/test-user-group-cert.config data/pam/ocserv.in data/pam/passdb.templ data/pam-single/passdb.templ \
	data/test-user-group-cert-no-pass.config data/test-cookie-timeout.config \
	data/test-cookie-timeout-2.config user-config-explicit/test data/test-explicit-ip.config \
	test-explicit-ip user-config-explicit/test2 user-config-explicit/test3 \
	user-config-explicit/test4 data/test-pass-opt-cert.config data/test-gssapi.config \
	data/test-ban.config data/test-sighup.config data/test-gssapi-local-map.config \
	data/test-cookie-invalidation.config data/test-enc-key2.config data/test-enc-key.config \
	certs/server-key-ossl.pem certs/server-key-p8.pem certs/user-cn.pem \
	certs/user-cert-testuser.pem test-stress data/test-user-config.config user-config/testuser \
	data/test-sighup-key-change.config data/test-sighup-key-change.config user-config/testipnet \
	certs/user-cert-testipnet.pem certs/user-cert-invalid.pem certs/server-cert-ca.pem \
	data/test-san-cert.config certs/user-san-cert.pem data/test-vhost3.passwd \
	certs/server-cert-ed25519.pem certs/server-key-ed25519.pem data/test-ed25519.config \
	certs/server-cert-rsa-pss.pem certs/server-key-rsa-pss.pem data/test-rsa-pss.config \
	data/test-otp-cert.config data/test-otp.oath test-otp-cert data/test-otp.passwd \
	data/test-otp.config data/test-cert-opt-pass.config data/test-gssapi-opt-pass.config \
	certs/server-key-secp521r1.pem certs/server-cert-secp521r1.pem data/test-vhost-pass-cert.config \
	data/vhost.hosts data/multiple-routes.config data/haproxy-auth.cfg data/test-haproxy-auth.config \
	data/haproxy-connect.cfg data/test-haproxy-connect.config scripts/vpnc-script \
	data/test-traffic.config data/test-compression-lzs.config data/test-compression-lz4.config \
	certs/crl.pem server-cert-rsa-pss data/test-gssapi-opt-cert.config data/test-ciphers.config \
	cipher-common.sh data/config-per-group.config data/config-per-group-url-cert.config \
	data/config-per-group-url-pass.config data/group-config/tost \
	data/raddb/access_reject data/raddb/accounting_response data/raddb/access_challenge data/raddb/acct_users \
	data/raddb/clients.conf data/raddb/radiusd.conf.in data/raddb/users \
	data/radiusclient/dictionary data/radiusclient/radiusclient.conf \
	data/radiusclient/servers data/radius.config data/radius-group.config data/radius-otp.config \
	data/test-udp-listen-host.config data/pam-kerberos/passdb.templ \
	data/test-max-same-1.config data/test-script-multi-user.config \
	sleep-connect-script data/test-psk-negotiate.config data/test-group-name.config \
	connect-ios-script data/apple-ios.config certs/kerberos-cert.pem \
	data/kdc.conf data/krb5.conf data/k5.KERBEROS.TEST data/kadm5.acl \
	data/ipv6-iface.config data/no-route-default.config data/no-route-group.config \
	data/group-config/group1 data/group-config/group2 data/test-namespace-listen.config data/disconnect-user.config \
	data/disconnect-user2.config data/ping-leases.config data/haproxy-proxyproto.config \
	data/haproxy-proxyproto.cfg scripts/proxy-connectscript data/haproxy-proxyproto-v1.config \
	data/haproxy-proxyproto-v1.cfg scripts/proxy-connectscript-v1 data/test-multiple-client-ip.config \
	data/test-client-bypass-protocol.config asan.supp certs/ca.tmpl certs/server-cert.tmpl \
	certs/user-cert.tmpl data/test-camouflage.config data/test-camouflage-norealm.config \
	data/radius-multi-group.config data/test-group-cert.config data/session-timeout.config \
	data/idle-timeout.config data/test-occtl.config data/vhost-traffic.config random-net.sh \
	random-net2.sh data/defvhost-traffic.config

xfail_scripts =
dist_check_SCRIPTS =  ocpasswd-test

if GNUTLS_WITH_NEW_CERTS
dist_check_SCRIPTS += server-cert-ed25519 server-cert-rsa-pss
endif


if ENABLE_ROOT_TESTS
#other root requiring tests
dist_check_SCRIPTS += haproxy-connect test-iroute test-multi-cookie test-pass-script \
	idle-timeout test-cookie-timeout test-cookie-timeout-2 test-explicit-ip \
	test-cookie-invalidation test-user-config test-append-routes test-ban \
	multiple-routes json test-udp-listen-host test-max-same-1 test-script-multi-user \
	apple-ios ipv6-iface test-namespace-listen disconnect-user disconnect-user2 \
	ping-leases test-ban-local test-client-bypass-protocol ipv6-small-net test-camouflage \
	test-camouflage-norealm vhost-traffic defvhost-traffic session-timeout test-occtl \
	no-ipv6-ocv3

if RADIUS_ENABLED
dist_check_SCRIPTS += radius-group radius-multi-group radius-otp
endif

dist_check_SCRIPTS += traffic lz4-compression lzs-compression \
	aes256-cipher aes128-cipher oc-aes256-gcm-cipher oc-aes128-gcm-cipher \
	test-config-per-group ac-aes128-gcm-cipher ac-aes256-gcm-cipher \
	no-dtls-cipher psk-negotiate psk-negotiate-match test-multiple-client-ip \
	test-config-per-group-url-pass test-config-per-group-url-cert

if RADIUS_ENABLED
dist_check_SCRIPTS += radius
endif

if RADIUS_ENABLED
dist_check_SCRIPTS += radius-config
endif
endif


if HAVE_CWRAP
if HAVE_CWRAP_ALL
dist_check_SCRIPTS += test-vhost
endif

dist_check_SCRIPTS += test-pass test-pass-cert test-cert test-group-pass \
	test-pass-group-cert test-pass-group-cert-no-pass test-sighup \
	test-enc-key test-sighup-key-change test-get-cert test-san-cert \
	test-gssapi test-pass-opt-cert test-cert-opt-pass test-gssapi-opt-pass \
	test-gssapi-opt-cert haproxy-auth test-maintenance resumption \
	test-group-name flowcontrol banner invalid-configs haproxy-proxyproto \
	haproxy-proxyproto-v1 drain-server drain-server-fail test-ignore-querystring-of-post \
	test-group-cert test-fork test-pass-svc test-cert-svc

if HAVE_CWRAP_PAM
dist_check_SCRIPTS += test-pam test-pam-noauth

if ENABLE_KERBEROS_TESTS
dist_check_SCRIPTS += kerberos
endif
endif

if HAVE_LIBOATH
dist_check_SCRIPTS += test-otp-cert test-otp
endif
endif

if ENABLE_TUN_TESTS
dist_check_SCRIPTS += no-route-default no-route-group
endif


AM_CPPFLAGS += \
	$(LIBOPTS_CFLAGS) \
	$(LIBTALLOC_CFLAGS) \
	$(CODE_COVERAGE_CFLAGS) \
	-I$(top_srcdir)/src/ \
	-I$(top_builddir)/src/ \
	-I$(top_srcdir)/src/common/ \
	-I$(top_builddir)/src/common/ \
	-I$(top_srcdir)/ \
	-I$(top_builddir)/

LDADD = $(LIBTALLOC_LIBS) ../src/libccan.a $(CODE_COVERAGE_LDFLAGS)

kkdcp_parsing_SOURCES = kkdcp-parsing.c
kkdcp_parsing_LDADD = $(LDADD)

cstp_recv_SOURCES = cstp-recv.c
cstp_recv_CFLAGS = $(CFLAGS) $(LIBGNUTLS_CFLAGS) $(LIBTALLOC_CFLAGS)
cstp_recv_LDADD = $(LDADD) $(LIBGNUTLS_LIBS)

json_escape_SOURCES = json-escape.c
json_escape_LDADD = $(LDADD)

url_escape_CPPFLAGS = $(AM_CPPFLAGS) -DUNDER_TEST
url_escape_SOURCES = url-escape.c
url_escape_LDADD = $(LDADD)

html_escape_CPPFLAGS = $(AM_CPPFLAGS) -DUNDER_TEST
html_escape_SOURCES = html-escape.c
html_escape_LDADD = $(LDADD)

ipv4_prefix_CPPFLAGS = $(AM_CPPFLAGS) -DUNDER_TEST
ipv4_prefix_SOURCES = ipv4-prefix.c
ipv4_prefix_LDADD = $(LDADD)

ban_ips_CPPFLAGS = $(AM_CPPFLAGS) -DUNDER_TEST
ban_ips_SOURCES = ban-ips.c
ban_ips_LDADD = $(LDADD)

str_test_SOURCES = str-test.c
str_test_LDADD = $(LDADD)

str_test2_SOURCES = str-test2.c
str_test2_LDADD = $(LDADD)

ipv6_prefix_CPPFLAGS = $(AM_CPPFLAGS) -DUNDER_TEST
ipv6_prefix_SOURCES = ipv6-prefix.c
ipv6_prefix_LDADD = $(LDADD)

human_addr_CPPFLAGS = $(AM_CPPFLAGS) -DUNDER_TEST
human_addr_SOURCES = human_addr.c
human_addr_LDADD = $(LDADD)


valid_hostname_LDADD = $(LDADD)

port_parsing_CPPFLAGS = $(AM_CPPFLAGS) -DUNDER_TEST
port_parsing_LDADD = $(LDADD)

check_PROGRAMS = str-test str-test2 ipv4-prefix ipv6-prefix kkdcp-parsing json-escape ban-ips \
	port-parsing human_addr valid-hostname url-escape html-escape cstp-recv \
	proxyproto-v1

gen_oidc_test_data_CPPFLAGS = $(AM_CPPFLAGS)
gen_oidc_test_data_SOURCES = generate_oidc_test_data.c
gen_oidc_test_data_LDADD = $(LDADD) $(CJOSE_LIBS) $(JANSSON_LIBS)

certs/ca.pem: certs/ca-key.pem certs/ca.tmpl
	certtool --generate-self-signed --template certs/ca.tmpl --load-privkey certs/ca-key.pem --outfile certs/ca.pem

certs/server-cert-ca.pem: certs/ca.pem certs/server-cert.pem
	cat certs/server-cert.pem certs/ca.pem > certs/server-cert-ca.pem

certs/server-cert.pem: certs/server-cert.tmpl certs/ca.pem certs/server-key.pem certs/ca-key.pem
	certtool --generate-certificate --template certs/server-cert.tmpl --load-privkey certs/server-key.pem --load-ca-certificate certs/ca.pem --load-ca-privkey certs/ca-key.pem --outfile certs/server-cert.pem

certs/user-cert.pem: certs/user-cert.tmpl certs/ca.pem certs/user-key.pem certs/ca-key.pem
	certtool --generate-certificate --template certs/user-cert.tmpl --load-privkey certs/user-key.pem --load-ca-certificate certs/ca.pem --load-ca-privkey certs/ca-key.pem --outfile certs/user-cert.pem

# make the user certificate invalid by signing it with another CA
certs/user-cert-invalid.pem: certs/user-cert.tmpl
	certtool --generate-privkey --outfile ca-key.tmp
	certtool --generate-self-signed --template certs/ca.tmpl --load-privkey ca-key.tmp --outfile ca.tmp
	certtool --generate-certificate --template certs/user-cert.tmpl --load-privkey certs/user-key.pem --load-ca-certificate ca.tmp --load-ca-privkey ca-key.tmp --outfile certs/user-cert-invalid.pem
	rm -f ca-key.tmp ca.tmp

if ENABLE_OIDC_AUTH_TESTS
check_PROGRAMS += gen_oidc_test_data
dist_check_SCRIPTS += test-oidc
endif

dist_check_SCRIPTS += test-owasp-headers

dist_check_SCRIPTS += test-replay

TESTS =  $(check_PROGRAMS) $(dist_check_SCRIPTS) $(xfail_scripts)

XFAIL_TESTS = $(xfail_scripts)

TESTS_ENVIRONMENT = srcdir="$(srcdir)" \
	top_builddir="$(top_builddir)" \
	LSAN_OPTIONS=suppressions=asan.supp

if DISABLE_ASAN_BROKEN_TESTS
TESTS_ENVIRONMENT += DISABLE_ASAN_BROKEN_TESTS=1
else
TESTS_ENVIRONMENT += DISABLE_ASAN_BROKEN_TESTS=0
endif
