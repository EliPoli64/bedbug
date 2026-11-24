-- ------------------------------------------------------
-- beDBug forward engineering script
-- Target engine: MySQL
-- ------------------------------------------------------

CREATE TABLE `public`.`a` (
  `id` bigint NOT NULL AUTO_INCREMENT,
  `c_id` bigint,
  PRIMARY KEY (`id`)
);

CREATE TABLE `public`.`b` (
  `id` bigint NOT NULL AUTO_INCREMENT,
  `a_id` bigint,
  PRIMARY KEY (`id`)
);

CREATE TABLE `public`.`c` (
  `id` bigint NOT NULL AUTO_INCREMENT,
  `b_id` bigint,
  PRIMARY KEY (`id`)
);

ALTER TABLE `public`.`a`
  ADD CONSTRAINT `fk_a_c` FOREIGN KEY (`c_id`)
  REFERENCES `public`.`c` (`id`);

ALTER TABLE `public`.`b`
  ADD CONSTRAINT `fk_b_a` FOREIGN KEY (`a_id`)
  REFERENCES `public`.`a` (`id`);

ALTER TABLE `public`.`c`
  ADD CONSTRAINT `fk_c_b` FOREIGN KEY (`b_id`)
  REFERENCES `public`.`b` (`id`);
