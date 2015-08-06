<?php
// $Id: pushkey.drush.inc,v 1.4 2010/12/02 23:17:44 greg1anderson Exp $

/**
 * @file
 * pushkey - Drush commands to create a public/private key pair
 * and push the public key to a remote account.
 *
 * Copyright (C) 2010  Greg Anderson <http://drupal.org/user/438598>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

function pushkey_drush_help($section) {
  switch ($section) {
    case 'drush:pushkey':
      return dt('Generate a public/private key pair and push to a remote machine.');
  }
}

function pushkey_drush_command() {
  $items = array();

  $items['pushkey'] = array(
    'description' => 'Generate a public/private key pair and push the public key to a remote server.',
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH, // No bootstrap.
    'options' => array(
      'keyname' => 'Specify which key to use or the name of the key to generate. Defaults to id_rsa.',
    ),
    'arguments' => array(
      'target' => 'The user and machine to push to ("user@machine.com").',
    ),
    'examples' => array(
      'drush pushkey user@myserver.com' => 'Push your public key to a remote server.',
      'drush pushkey user@myserver.com --keyname=drupal_rsa' => 'Push your public key named drupal_rsa.pub to a remote server.',
    ),
  );

  return $items;
}

function drush_pushkey($target = NULL) {
  // Make sure that we were called with a parameter
  if ($target == NULL) {
    return drush_set_error('DRUSH_NO_TARGET', dt('No target was specified.'));
  }

  if (is_null(drush_server_home())) {
    return drush_set_error('DRUSH_NO_HOME', dt('Cannot determine appropriate home directory.'));
  }
  
  $local_machine_name = exec('uname -n');
  
  //
  // If there is a proxy, then we will first push
  // the key to the proxy, and then we will tell
  // the proxy to push the key to the final target.
  // (This is not supported yet.)
  //
  $proxy = drush_get_option('proxy');
  if (isset($proxy)) {
    $final_target = $target;
    $target = $proxy;
  }
  
  $target_record = drush_extras_parse_target($target);
  if (!$target_record) {
    return FALSE;
  }
  $remote_user = $target_record['remote-user'];
  $remote_machine = $target_record['remote-host'];
  
  // If the user was not specified with the target,
  // then assume that the remote user == local user
  if (!isset($remote_user)) {
    $remote_user = exec('whoami');
  }
  
  // Figure out where the remote home directory is
  $remote_machine_with_user = "$remote_user@$remote_machine";
  
  // Make sure that the .ssh directory exists
  $ssh_dir = drush_server_home() . '/.ssh';
  if (!is_dir($ssh_dir)) {
    mkdir($ssh_dir);
  }

  // If we do not have an id_rsa file, then make one
  $private_key_file = $ssh_dir . '/' . drush_get_option('keyname', 'id_rsa');
  $public_key_file = $ssh_dir . '/' . drush_get_option('keyname',  'id_rsa') . '.pub';
  
  if (!is_file($private_key_file)) {
    drush_op('system', 'ssh-keygen -t rsa -f ' . $private_key_file);
  }
  
  // Read in the public key data
  if (is_file($public_key_file)) {
    $public_key_data = trim(file_get_contents($public_key_file));
  }
  elseif (drush_get_context('DRUSH_SIMULATE')) {
    $public_key_data = "SIMULATED_PUBLIC_KEY_DATA";
  }
  
  // Object if we still do not have a public key file
  if (!isset($public_key_data)) {
    return drush_set_error('DRUSH_NO_KEY', dt('Could not find or create a public / private key pair.'));
  }

  // Read in the public key data
  $remote_public_key_file = $local_machine_name . '_' . drush_get_option('keyname', 'id_rsa') . '.pub';
  
  // Build the remote command and then run it
  $exec = "ssh $remote_machine_with_user /bin/sh -c \"'mkdir -p \\\$HOME/.ssh && key=\\\"$public_key_data\\\" && (echo \\\$key > \\\$HOME/.ssh/$remote_public_key_file) && (cat \\\$HOME/.ssh/$remote_public_key_file >> \\\$HOME/.ssh/authorized_keys) && chmod 600 \\\$HOME/.ssh/authorized_keys'\"";
  $result = drush_op('system', $exec); 
}

function drush_extras_parse_target($target) {
  $remote_machine = $target;
  $at_pos = strpos($target, '@');
  if ($at_pos !== FALSE) {
    // If the '@' is at the beginning, then the target
    // is an alias.
    if ($at_pos == 0) {
      $alias_record = drush_sitealias_get_record($target);
      if (empty($alias_record)) {
        return drush_set_error('DRUSH_NO_ALIAS', dt('Could not find the alias !alias.', array('!alias' => $alias)));
      }
      if (empty($alias_record)) {
        return drush_set_error('DRUSH_NO_ALIAS', dt('Could not find the alias !alias.', array('!alias' => $alias)));
      }
      if (array_key_exists('remote-user', $alias_record)) {
        $remote_user = $alias_record['remote-user'];
      }
      if (array_key_exists('remote-host', $alias_record)) {
        $remote_machine = $alias_record['remote-host'];
      }
      else {
        return drush_set_error('DRUSH_NOT_REMOTE', dt('The alias !alias is not remote, and therefore cannot be used as a target to pushkey.', array('!alias' => $alias)));
      }
    }
    // If there is an '@' somewhere else, then the username
    // comes before the '@'
    else {
      $remote_user = substr($target, 0, $at_pos);
      $remote_machine = substr($target, $at_pos + 1);
    }
  }
  
  return array('remote-user' => $remote_user, 'remote-host' => $remote_machine);
}
