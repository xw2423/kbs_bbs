CREATE TABLE IF NOT EXISTS `board_user` (
  `board` varchar(32) COLLATE gbk_bin NOT NULL,
  `user` varchar(32) COLLATE gbk_bin NOT NULL,
  `time` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  `status` int(11) NOT NULL,
  `manager` varchar(32) COLLATE gbk_bin NOT NULL,
  `score` int(11) NOT NULL,
  `flag` bigint(20) NOT NULL,
  UNIQUE KEY `member` (`board`,`user`),
  KEY `board` (`board`),
  KEY `user` (`user`),
  KEY `time` (`time`),
  KEY `flag` (`flag`),
  KEY `status` (`status`),
  KEY `score` (`score`)
) ENGINE=InnoDB DEFAULT CHARSET=gbk COLLATE=gbk_bin;