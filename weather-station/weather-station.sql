CREATE TABLE IF NOT EXISTS `weather` (
  `ID` int(14) NOT NULL AUTO_INCREMENT,
  `dt` datetime NOT NULL DEFAULT current_timestamp(),
  `ts` int(14) NOT NULL,
  `rain` int(5) NOT NULL,
  `light` int(5) NOT NULL,
  `temp` int(6) NOT NULL,
  `press` int(11) NOT NULL,
  `alt` int(5) NOT NULL,
  `humi` int(5) NOT NULL,
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB;
