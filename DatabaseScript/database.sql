/*
Navicat MySQL Data Transfer

Source Server         : mysql
Source Server Version : 50518
Source Host           : localhost:3306
Source Database       : siri_proxy

Target Server Type    : MYSQL
Target Server Version : 50518
File Encoding         : 65001

Date: 2012-01-16 14:55:17
*/

SET FOREIGN_KEY_CHECKS=0;

-- ----------------------------
-- Table structure for `acs_logs`
-- ----------------------------
DROP TABLE IF EXISTS `acs_logs`;
CREATE TABLE `acs_logs` (
  `auto_sequece` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `key_id` varchar(100) NOT NULL,
  `peer_ipaddress` varchar(50) NOT NULL,
  `time_stamp` bigint(20) NOT NULL,
  `time_stamp_string` varchar(50) DEFAULT NULL,
  PRIMARY KEY (`auto_sequece`),
  KEY `fk_keys_keyid` (`key_id`),
  CONSTRAINT `fk_keys_keyid` FOREIGN KEY (`key_id`) REFERENCES `allkeys` (`key_id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB AUTO_INCREMENT=27 DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of acs_logs
-- ----------------------------

-- ----------------------------
-- Table structure for `allkeys`
-- ----------------------------
DROP TABLE IF EXISTS `allkeys`;
CREATE TABLE `allkeys` (
  `key_id` varchar(100) NOT NULL,
  `hints` int(10) unsigned zerofill NOT NULL DEFAULT '0000000000',
  `remark` varchar(100) DEFAULT NULL,
  PRIMARY KEY (`key_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of allkeys
-- ----------------------------

-- ----------------------------
-- Table structure for `tickets`
-- ----------------------------
DROP TABLE IF EXISTS `tickets`;
CREATE TABLE `tickets` (
  `assistantId` varchar(100) NOT NULL,
  `x_ace_host` varchar(100) DEFAULT NULL,
  `user_agent` varchar(100) NOT NULL,
  `speechId` varchar(100) NOT NULL,
  `sessionValidationData` blob NOT NULL,
  `expired` int(11) NOT NULL DEFAULT '0',
  `modify` bigint(20) NOT NULL DEFAULT '0',
  `modify_date_string` varchar(50) DEFAULT NULL,
  `session_validate_date` bigint(20) NOT NULL,
  `session_validate_date_string` varchar(50) DEFAULT NULL,
  PRIMARY KEY (`assistantId`),
  UNIQUE KEY `unique_assistantId_index` (`assistantId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of tickets
-- ----------------------------

-- ----------------------------
-- View structure for `vkeylogs`
-- ----------------------------
DROP VIEW IF EXISTS `vkeylogs`;
CREATE ALGORITHM=UNDEFINED DEFINER=`root`@`localhost` SQL SECURITY DEFINER VIEW `vkeylogs` AS select `allkeys`.`key_id` AS `key_id`,`allkeys`.`hints` AS `hints`,`allkeys`.`remark` AS `remark`,`acs_logs`.`auto_sequece` AS `auto_sequece`,`acs_logs`.`peer_ipaddress` AS `peer_ipaddress`,`acs_logs`.`time_stamp` AS `time_stamp`,`acs_logs`.`time_stamp_string` AS `time_stamp_string` from (`allkeys` join `acs_logs` on((`allkeys`.`key_id` = `acs_logs`.`key_id`))) ;
DROP TRIGGER IF EXISTS `on_insert_trigger`;
DELIMITER ;;
CREATE TRIGGER `on_insert_trigger` AFTER INSERT ON `acs_logs` FOR EACH ROW UPDATE allkeys SET hints=hints+1 WHERE allkeys.key_id=NEW.key_id
;;
DELIMITER ;
