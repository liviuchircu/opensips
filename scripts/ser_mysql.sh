#!/bin/sh 
#
# $Id$
#
# Script for adding and dropping ser MySql tables
# 
# TO-DO: update_structures command for migriting to new
#        table definitons
# USAGE: call the command without any parameters for info
#
# History:
#
# 2003-01-24: Changed realm column of subscriber and pending
#             tables to domain (janakj)
# 2003-01-25: Optimized keys of some core tables (janakj)
#

#################################################################
# config vars
#################################################################
DBNAME=ser
DBHOST=localhost
USERNAME=ser
DEFAULT_PW=heslo
ROUSER=serro
RO_PW=47serro11
CMD="mysql -h $DBHOST -p -u "
BACKUP_CMD="mysqldump -h $DBHOST -p -c -t -u "
TABLE_TYPE="TYPE=MyISAM"
SQL_USER="root"

#################################################################


usage() {
COMMAND=`basename $0`
cat <<EOF
usage: $COMMAND create
       $COMMAND drop   (!!entirely deletes tables)
       $COMMAND reinit (!!entirely deletes and than re-creates tables
       $COMMAND backup (dumps current database to stdout)
	   $COMMAND restore <file> (restores tables from a file)
       $COMMAND copy <new_db> (creates a new db from an existing one)

       if you want to manipulate database as other MySql user than
       root, want to change database name from default value "$DBNAME",
       or want to use other values for users and password, edit the
       "config vars" section of the command $COMMAND

EOF
} #usage

ser_backup()  # pars: <database name> <sql_user>
{
if [ $# -ne 2 ] ; then
	echo "ser_drop function takes two params"
	exit 1
fi
$BACKUP_CMD $2 $1
}

ser_restore() #pars: <database name> <sql_user> <filename>
{
if [ $# -ne 3 ] ; then
	echo "ser_drop function takes two params"
	exit 1
fi
$CMD $2 $1 < $3
}

ser_drop()  # pars: <database name> <sql_user>
{
if [ $# -ne 2 ] ; then
	echo "ser_drop function takes two params"
	exit 1
fi

$CMD $2 << EOF
drop database $1;
EOF
} #ser_drop

ser_create () # pars: <database name> <sql_user>
{

#test
#cat > /tmp/sss <<EOF

if [ $# -ne 2 ] ; then
	echo "ser_create function takes two params"
	exit 1
fi

echo "creating database $1 ..."

$CMD $2 <<EOF
create database $1;
use $1;

# Users: ser is the regular user, serro only for reading
GRANT ALL PRIVILEGES ON $1.* TO $USERNAME IDENTIFIED  BY '$DEFAULT_PW';
GRANT ALL PRIVILEGES ON $1.* TO ${USERNAME}@$DBHOST IDENTIFIED BY '$DEFAULT_PW';
GRANT SELECT ON $1.* TO $ROUSER IDENTIFIED BY '$RO_PW';
GRANT SELECT ON $1.* TO ${ROUSER}@$DBHOST IDENTIFIED BY '$RO_PW';


#
# Table structure versions
#

CREATE TABLE version (
   table_name varchar(64) NOT NULL,
   version smallint(5) DEFAULT '0' NOT NULL
) $TABLE_TYPE;

#
# Dumping data for table 'version'
#

INSERT INTO version VALUES ( 'subscriber', '1');
INSERT INTO version VALUES ( 'reserved', '1');
INSERT INTO version VALUES ( 'phonebook', '1');
INSERT INTO version VALUES ( 'pending', '1');
INSERT INTO version VALUES ( 'missed_calls', '1');
INSERT INTO version VALUES ( 'location', '2');
INSERT INTO version VALUES ( 'grp', '2');
INSERT INTO version VALUES ( 'event', '1');
INSERT INTO version VALUES ( 'aliases', '2');
INSERT INTO version VALUES ( 'active_sessions', '1');
INSERT INTO version VALUES ( 'acc', '1');
INSERT INTO version VALUES ( 'config', '1');
INSERT INTO version VALUES ( 'silo', '1');
INSERT INTO version VALUES ( 'domain', '1');
INSERT INTO version VALUES ( 'uri', '1');


#
# Table structure for table 'acc' -- accounted calls
#


CREATE TABLE acc (
  sip_from varchar(128) NOT NULL default '',
  sip_to varchar(128) NOT NULL default '',
  sip_status varchar(128) NOT NULL default '',
  sip_method varchar(16) NOT NULL default '',
  i_uri varchar(128) NOT NULL default '',
  o_uri varchar(128) NOT NULL default '',
  sip_callid varchar(128) NOT NULL default '',
  user varchar(64) NOT NULL default '',
  time datetime NOT NULL default '0000-00-00 00:00:00',
  timestamp timestamp(14) NOT NULL
) $TABLE_TYPE;




#
# Table structure for table 'active_sessions' -- web stuff
#


CREATE TABLE active_sessions (
  sid varchar(32) NOT NULL default '',
  name varchar(32) NOT NULL default '',
  val text,
  changed varchar(14) NOT NULL default '',
  PRIMARY KEY  (name,sid),
  KEY changed (changed)
) $TABLE_TYPE;




#
# Table structure for table 'aliases' -- location-like table
#


CREATE TABLE aliases (
  user varchar(50) NOT NULL default '',
  domain varchar(100) NOT NULL default '',
  contact varchar(255) NOT NULL default '',
  expires datetime default NULL,
  q float(10,2) default NULL,
  callid varchar(255) default NULL,
  cseq int(11) default NULL,
  last_modified timestamp(14) NOT NULL,
  PRIMARY KEY user (user, domain, contact)
) $TABLE_TYPE;




#
# Table structure for table 'event' -- track of predefined
# events to which a user subscribed
#


CREATE TABLE event (
  id int(10) unsigned NOT NULL auto_increment,
  user varchar(50) NOT NULL default '',
  uri varchar(255) NOT NULL default '',
  description varchar(255) NOT NULL default '',
  PRIMARY KEY (id)
) $TABLE_TYPE;




#
# Table structure for table 'grp' -- group membership
# table; used primarily for ACLs
#


CREATE TABLE grp (
  user varchar(50) NOT NULL default '',
  domain varchar(100) NOT NULL default '',
  grp varchar(50) NOT NULL default '',
  last_modified datetime NOT NULL default '0000-00-00 00:00:00',
  PRIMARY KEY(user, domain, grp)
) $TABLE_TYPE;




#
# Table structure for table 'location' -- that is persistent UsrLoc
#


CREATE TABLE location (
  user varchar(50) NOT NULL default '',
  domain varchar(100) NOT NULL default '',
  contact varchar(255) NOT NULL default '',
  expires datetime default NULL,
  q float(10,2) default NULL,
  callid varchar(255) default NULL,
  cseq int(11) default NULL,
  last_modified timestamp(14) NOT NULL,
  PRIMARY KEY(user, domain, contact)
) $TABLE_TYPE;




#
# Table structure for table 'missed_calls' -- acc-like table
# for keeping track of missed calls
#


CREATE TABLE missed_calls (
  sip_from varchar(128) NOT NULL default '',
  sip_to varchar(128) NOT NULL default '',
  sip_status varchar(128) NOT NULL default '',
  sip_method varchar(16) NOT NULL default '',
  i_uri varchar(128) NOT NULL default '',
  o_uri varchar(128) NOT NULL default '',
  sip_callid varchar(128) NOT NULL default '',
  user varchar(64) NOT NULL default '',
  time datetime NOT NULL default '0000-00-00 00:00:00',
  timestamp timestamp(14) NOT NULL
) $TABLE_TYPE;




#
# Table structure for table 'pending' -- unconfirmed subscribtion
# requests
#


CREATE TABLE pending (
  phplib_id varchar(32) NOT NULL default '',
  USER_ID varchar(100) NOT NULL default '',
  PASSWORD varchar(25) NOT NULL default '',
  FIRST_NAME varchar(25) NOT NULL default '',
  LAST_NAME varchar(45) NOT NULL default '',
  PHONE varchar(15) NOT NULL default '',
  EMAIL_ADDRESS varchar(50) NOT NULL default '',
  DATETIME_CREATED datetime NOT NULL default '0000-00-00 00:00:00',
  DATETIME_MODIFIED datetime NOT NULL default '0000-00-00 00:00:00',
  confirmation varchar(64) NOT NULL default '',
  flag char(1) NOT NULL default 'o',
  SendNotification varchar(50) NOT NULL default '',
  Greeting varchar(50) NOT NULL default '',
  HA1 varchar(128) NOT NULL default '',
  domain varchar(128) NOT NULL default '',
  ha1b varchar(128) NOT NULL default '',
  perms varchar(32) default NULL,
  allow_find char(1) NOT NULL default '0',
  timezone varchar(128) default NULL,
  PRIMARY KEY (USER_ID, domain),
  KEY USER_ID_2 (USER_ID),
  UNIQUE KEY phplib_id (phplib_id)
) $TABLE_TYPE;




#
# Table structure for table 'phonebook' -- user's phonebook
#


CREATE TABLE phonebook (
  id int(10) unsigned NOT NULL auto_increment,
  user varchar(50) NOT NULL default '',
  fname varchar(32) NOT NULL default '',
  lname varchar(32) NOT NULL default '',
  sip_uri varchar(128) NOT NULL default '',
  PRIMARY KEY  (id)
) $TABLE_TYPE;




#
# Table structure for table 'reserved' -- reserved username
# which should be never allowed for subscription
#


CREATE TABLE reserved (
  user_id char(100) NOT NULL default '',
  UNIQUE KEY user_id (user_id)
) $TABLE_TYPE;




#
# Table structure for table 'subscriber' -- user database
#


CREATE TABLE subscriber (
  phplib_id varchar(32) NOT NULL default '',
  USER_ID varchar(100) NOT NULL default '',
  PASSWORD varchar(25) NOT NULL default '',
  FIRST_NAME varchar(25) NOT NULL default '',
  LAST_NAME varchar(45) NOT NULL default '',
  PHONE varchar(15) NOT NULL default '',
  EMAIL_ADDRESS varchar(50) NOT NULL default '',
  DATETIME_CREATED datetime NOT NULL default '0000-00-00 00:00:00',
  DATETIME_MODIFIED datetime NOT NULL default '0000-00-00 00:00:00',
  confirmation varchar(64) NOT NULL default '',
  flag char(1) NOT NULL default 'o',
  SendNotification varchar(50) NOT NULL default '',
  Greeting varchar(50) NOT NULL default '',
  HA1 varchar(128) NOT NULL default '',
  domain varchar(128) NOT NULL default '',
  ha1b varchar(128) NOT NULL default '',
  perms varchar(32) default NULL,
  allow_find char(1) NOT NULL default '0',
  timezone varchar(128) default NULL,
  UNIQUE KEY phplib_id (phplib_id),
  PRIMARY KEY (USER_ID, domain),
  KEY USER_ID_2 (USER_ID)
) $TABLE_TYPE;

# hook-table for all posssible future config values
# (currently unused)

CREATE TABLE config (
   attribute varchar(32) NOT NULL,
   value varchar(128) NOT NULL,
   user varchar(100) NOT NULL default '',
   modified timestamp(14)
) $TABLE_TYPE;

# "instant" message silo

CREATE TABLE silo(
	mid INTEGER NOT NULL AUTO_INCREMENT PRIMARY KEY,
	src_addr VARCHAR(255) NOT NULL DEFAULT "",
	dst_addr VARCHAR(255) NOT NULL DEFAULT "",
	r_uri VARCHAR(255) NOT NULL DEFAULT "",
	inc_time INTEGER NOT NULL DEFAULT 0,
	exp_time INTEGER NOT NULL DEFAULT 0,
	ctype VARCHAR(32) NOT NULL DEFAULT "text/plain",
	body BLOB NOT NULL DEFAULT ""
) $TABLE_TYPE ;


#
# Table structure for table 'domain' -- domains proxy is responsible for
#

CREATE TABLE domain (
  domain varchar(50) NOT NULL default '',
  last_modified datetime NOT NULL default '0000-00-00 00:00:00',
  PRIMARY KEY  (domain)
) $TABLE_TYPE;


#
# Table structure for table 'uri' -- uri user parts users are allowed to use
#
CREATE TABLE uri (
  user varchar(50) NOT NULL default '',
  domain varchar(50) NOT NULL default '',
  uri_user varchar(50) NOT NULL default '',
  last_modified datetime NOT NULL default '0000-00-00 00:00:00',
  PRIMARY KEY (user, domain, uri_user)
) $TABLE_TYPE;


# add an admin user "admin" with password==heslo, 
# so that one can try it out on quick start

INSERT INTO subscriber (USER_ID, PASSWORD, FIRST_NAME, LAST_NAME, PHONE, 
	EMAIL_ADDRESS, DATETIME_CREATED, DATETIME_MODIFIED, confirmation, 
	flag, SendNotification, Greeting, HA1, domain, ha1b, phplib_id, perms ) 
	VALUES ( 'admin', 'heslo', 'Initial', 'Admin', '123', 
	'root@localhost', '2002-09-04 19:37:45', '0000-00-00 00:00:00', 
	'57DaSIPuCm52UNe54LF545750cfdL48OMZfroM53', 'o', '', '', 
	'0239482f19d262f3953186a725a6f53b', 'iptel.org', 
	'a84e8abaa7e83d1b45c75ab15b90c320', '65e397cda0aa8e3202ea22cbd350e4e9', 'admin' );


EOF

} # ser_create


case $1 in
	copy)
		shift
		if [ $# -ne 1 ]; then
			usage
			exit 1
		fi
		tmp_file=/tmp/ser_mysql.$$
		ser_backup $DBNAME $SQL_USER > $tmp_file
		ret=$?
		if [ "$ret" -ne 0 ]; then
			rm $tmp_file
			exit $ret
		fi
		ser_create $1 $SQL_USER
		ret=$?
		if [ "$ret" -ne 0 ]; then
			rm $tmp_file
			exit $ret
		fi
		ser_restore $1 $SQL_USER $tmp_file
		ret=$?
		rm $tmp_file
		exit $ret
		;;
	backup)
		ser_backup $DBNAME $SQL_USER
		exit $?
		;;
	restore)
		shift
		if [ $# -ne 1 ]; then
			usage
			exit 1
		fi
		ser_restore $DBNAME $SQL_USER $1
		exit $?
		;;
	create)
		shift
		if [ $# -eq 1 ] ; then
			DBNAME="$1"
		fi
		ser_create $DBNAME $SQL_USER
		exit $?
		;;
	drop)
		ser_drop $DBNAME $SQL_USER
		exit $?
		;;
	reinit)
		ser_drop $DBNAME $SQL_USER
		ret=$?
		if [ "$ret" -ne 0 ]; then
			exit $ret
		fi
		ser_create $DBNAME $SQL_USER
		exit $?
		;;
	*)
		usage
		exit 1;
		;;
esac

