-- ------------------------------------------------------
-- beDBug forward engineering script
-- Target engine: MySQL
-- ------------------------------------------------------

CREATE TABLE `public`.`customer` (
  `id` bigint NOT NULL AUTO_INCREMENT,
  `name` varchar(80) NOT NULL,
  `email` varchar(255) NOT NULL UNIQUE,
  `status` enum('active', 'inactive') NOT NULL DEFAULT 'active',
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT 'Row creation time.',
  PRIMARY KEY (`id`)
);

CREATE TABLE `public`.`orders` (
  `id` bigint NOT NULL AUTO_INCREMENT,
  `customer_id` bigint NOT NULL,
  `total` decimal(12,2) NOT NULL DEFAULT 0,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`)
);

ALTER TABLE `public`.`orders`
  ADD CONSTRAINT `fk_orders_customer` FOREIGN KEY (`customer_id`)
  REFERENCES `public`.`customer` (`id`) ON DELETE CASCADE ON UPDATE RESTRICT;

CREATE INDEX `ix_customer_created_at` ON `public`.`customer` (`created_at`);
