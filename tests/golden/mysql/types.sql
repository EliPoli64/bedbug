-- ------------------------------------------------------
-- beDBug forward engineering script
-- Target engine: MySQL
-- ------------------------------------------------------

CREATE TABLE `public`.`types_demo` (
  `c_int` int,
  `c_bigint` bigint,
  `c_smallint` smallint,
  `c_varchar` varchar(100),
  `c_char` char(2),
  `c_text` text,
  `c_bool` tinyint(1),
  `c_date` date,
  `c_time` time,
  `c_ts` timestamp,
  `c_dec` decimal(12,3),
  `c_float` float,
  `c_double` double,
  `c_blob` blob,
  `c_json` json,
  `c_uuid` char(36),
  `c_enum` enum('red', 'green', 'blue'),
  `weird"name` varchar(5)
);
