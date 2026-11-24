-- ------------------------------------------------------
-- beDBug forward engineering script
-- Target engine: SQL Server
-- ------------------------------------------------------

CREATE TABLE [public].[a] (
  [id] bigint NOT NULL IDENTITY(1,1),
  [c_id] bigint,
  PRIMARY KEY ([id])
);

CREATE TABLE [public].[b] (
  [id] bigint NOT NULL IDENTITY(1,1),
  [a_id] bigint,
  PRIMARY KEY ([id])
);

CREATE TABLE [public].[c] (
  [id] bigint NOT NULL IDENTITY(1,1),
  [b_id] bigint,
  PRIMARY KEY ([id])
);

ALTER TABLE [public].[a]
  ADD CONSTRAINT [fk_a_c] FOREIGN KEY ([c_id])
  REFERENCES [public].[c] ([id]);

ALTER TABLE [public].[b]
  ADD CONSTRAINT [fk_b_a] FOREIGN KEY ([a_id])
  REFERENCES [public].[a] ([id]);

ALTER TABLE [public].[c]
  ADD CONSTRAINT [fk_c_b] FOREIGN KEY ([b_id])
  REFERENCES [public].[b] ([id]);
