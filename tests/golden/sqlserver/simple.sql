-- ------------------------------------------------------
-- beDBug forward engineering script
-- Target engine: SQL Server
-- ------------------------------------------------------

CREATE TABLE [public].[customer] (
  [id] bigint NOT NULL IDENTITY(1,1),
  [name] nvarchar(80) NOT NULL,
  [email] nvarchar(255) NOT NULL UNIQUE,
  [status] nvarchar(255) NOT NULL DEFAULT 'active',
  [created_at] datetime2 NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY ([id])
);

CREATE TABLE [public].[orders] (
  [id] bigint NOT NULL IDENTITY(1,1),
  [customer_id] bigint NOT NULL,
  [total] decimal(12,2) NOT NULL DEFAULT 0,
  [created_at] datetime2 NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY ([id])
);

ALTER TABLE [public].[orders]
  ADD CONSTRAINT [fk_orders_customer] FOREIGN KEY ([customer_id])
  REFERENCES [public].[customer] ([id]) ON DELETE CASCADE ON UPDATE NO ACTION;

CREATE INDEX [ix_customer_created_at] ON [public].[customer] ([created_at]);
