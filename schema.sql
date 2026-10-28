-- Banking OOP - bootstrap schema for a fresh Turso Cloud libSQL database.
-- Safe to run again for existing objects; this file does not migrate older schemas.
-- Apply this file only to a new/empty database. Use versioned migrations for live schemas.

CREATE TABLE IF NOT EXISTS users (
    user_id TEXT PRIMARY KEY,
    username TEXT NOT NULL UNIQUE,
    password_hash TEXT NOT NULL,
    salt TEXT NOT NULL,
    full_name TEXT NOT NULL,
    email TEXT NOT NULL UNIQUE,
    cccd TEXT NOT NULL UNIQUE,
    phone TEXT NOT NULL UNIQUE,
    role TEXT NOT NULL CHECK (role IN ('CUSTOMER', 'TELLER', 'ADMIN')),
    status TEXT NOT NULL DEFAULT 'ACTIVE' CHECK (status IN ('ACTIVE', 'BLOCKED')),
    pin_hash TEXT NOT NULL DEFAULT '',
    failed_attempts INTEGER NOT NULL DEFAULT 0 CHECK (failed_attempts >= 0),
    locked_until INTEGER NOT NULL DEFAULT 0,
    pin_failed_attempts INTEGER NOT NULL DEFAULT 0 CHECK (pin_failed_attempts >= 0),
    pin_locked_until INTEGER NOT NULL DEFAULT 0,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS accounts (
    account_id TEXT PRIMARY KEY,
    user_id TEXT NOT NULL,
    balance_vnd INTEGER NOT NULL DEFAULT 0 CHECK (balance_vnd >= 0),
    account_type TEXT NOT NULL CHECK (account_type IN ('CHECKING', 'SAVINGS', 'LOAN', 'CREDIT')),
    status TEXT NOT NULL DEFAULT 'ACTIVE' CHECK (status IN ('ACTIVE', 'BLOCKED', 'CLOSED')),
    interest_rate REAL NOT NULL DEFAULT 0 CHECK (interest_rate >= 0),
    last_tx_id TEXT NOT NULL DEFAULT '',
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES users(user_id)
);

CREATE TABLE IF NOT EXISTS transactions (
    transaction_id TEXT PRIMARY KEY,
    sender_account_id TEXT NOT NULL,
    receiver_account_id TEXT NOT NULL,
    amount_vnd INTEGER NOT NULL CHECK (amount_vnd > 0),
    fee_vnd INTEGER NOT NULL DEFAULT 0 CHECK (fee_vnd >= 0),
    type TEXT NOT NULL,
    status TEXT NOT NULL CHECK (status IN ('PENDING', 'SUCCESS', 'FAILED')),
    description TEXT,
    post_balance_vnd INTEGER NOT NULL,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS audit_logs (
    log_id TEXT PRIMARY KEY,
    operator_id TEXT NOT NULL,
    action TEXT NOT NULL,
    target_id TEXT NOT NULL,
    details TEXT,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS system_config (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

-- Revolving credit applications and approved terms. CREDIT account balance_vnd
-- stores the outstanding principal; the limit and terms live here.
CREATE TABLE IF NOT EXISTS credit_applications (
    application_id TEXT PRIMARY KEY,
    user_id TEXT NOT NULL,
    requested_limit_vnd INTEGER NOT NULL CHECK (requested_limit_vnd >= 1000000),
    monthly_income_vnd INTEGER NOT NULL DEFAULT 0 CHECK (monthly_income_vnd >= 0),
    status TEXT NOT NULL DEFAULT 'PENDING' CHECK (status IN ('PENDING', 'APPROVED', 'REJECTED')),
    decided_by TEXT NOT NULL DEFAULT '',
    decision_note TEXT NOT NULL DEFAULT '',
    account_id TEXT NOT NULL DEFAULT '',
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    decided_at TEXT,
    FOREIGN KEY (user_id) REFERENCES users(user_id)
);
CREATE INDEX IF NOT EXISTS idx_credit_applications_user ON credit_applications(user_id, created_at DESC);
CREATE UNIQUE INDEX IF NOT EXISTS idx_credit_applications_one_pending
    ON credit_applications(user_id) WHERE status = 'PENDING';
CREATE TABLE IF NOT EXISTS credit_lines (
    account_id TEXT PRIMARY KEY,
    credit_limit_vnd INTEGER NOT NULL CHECK (credit_limit_vnd >= 1000000),
    monthly_rate_percent REAL NOT NULL CHECK (monthly_rate_percent >= 0),
    due_day INTEGER NOT NULL CHECK (due_day BETWEEN 1 AND 28),
    minimum_payment_percent REAL NOT NULL CHECK (minimum_payment_percent BETWEEN 1 AND 100),
    opened_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    last_interest_date TEXT NOT NULL DEFAULT '1970-01-01',
    FOREIGN KEY (account_id) REFERENCES accounts(account_id)
);

CREATE TABLE IF NOT EXISTS credit_application_documents (
    document_id TEXT PRIMARY KEY,
    application_id TEXT NOT NULL UNIQUE,
    user_id TEXT NOT NULL,
    file_name TEXT NOT NULL,
    mime_type TEXT NOT NULL,
    file_size INTEGER NOT NULL CHECK (file_size > 0 AND file_size <= 5242880),
    storage_key TEXT NOT NULL,
    status TEXT NOT NULL DEFAULT 'PENDING_REVIEW'
        CHECK (status IN ('PENDING_REVIEW', 'APPROVED', 'REJECTED')),
    review_note TEXT NOT NULL DEFAULT '',
    reviewed_by TEXT,
    submitted_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (application_id) REFERENCES credit_applications(application_id) ON DELETE CASCADE,
    FOREIGN KEY (user_id) REFERENCES users(user_id)
);
CREATE INDEX IF NOT EXISTS idx_credit_application_documents_status
    ON credit_application_documents(status, submitted_at DESC);

-- The application looks up an idempotency key without user_id, so the key is
-- globally unique in this schema.
CREATE TABLE IF NOT EXISTS idempotency_keys (
    idempotency_key TEXT PRIMARY KEY,
    user_id TEXT NOT NULL,
    action TEXT NOT NULL,
    response_json TEXT NOT NULL,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES users(user_id)
);

CREATE TABLE IF NOT EXISTS daily_transaction_limits (
    user_id TEXT NOT NULL,
    txn_date TEXT NOT NULL,
    total_amount_vnd INTEGER NOT NULL DEFAULT 0
        CHECK (total_amount_vnd >= 0 AND total_amount_vnd <= 1000000000),
    PRIMARY KEY (user_id, txn_date),
    FOREIGN KEY (user_id) REFERENCES users(user_id)
);

CREATE TABLE IF NOT EXISTS bills (
    bill_id TEXT PRIMARY KEY,
    provider_id TEXT NOT NULL,
    provider_name TEXT NOT NULL,
    customer_code TEXT NOT NULL,
    customer_name TEXT NOT NULL,
    amount_vnd INTEGER NOT NULL CHECK (amount_vnd > 0),
    service_type TEXT NOT NULL CHECK (
        service_type IN ('ELECTRICITY', 'WATER', 'INTERNET', 'MOBILE_POSTPAID', 'TUITION')
    ),
    status TEXT NOT NULL DEFAULT 'UNPAID' CHECK (status IN ('UNPAID', 'PAID', 'EXPIRED')),
    due_date TEXT NOT NULL,
    period TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS savings_accounts (
    savings_id TEXT PRIMARY KEY,
    user_id TEXT NOT NULL,
    source_account_id TEXT NOT NULL,
    principal_vnd INTEGER NOT NULL CHECK (principal_vnd >= 1000000),
    interest_rate REAL NOT NULL CHECK (interest_rate >= 0),
    term_months INTEGER NOT NULL CHECK (term_months > 0),
    term_days INTEGER NOT NULL DEFAULT 0 CHECK (term_days >= 0),
    maturity_option TEXT NOT NULL CHECK (
        maturity_option IN ('RENEW_ALL', 'RENEW_PRINCIPAL', 'CLOSE_TO_ACCOUNT')
    ),
    status TEXT NOT NULL DEFAULT 'ACTIVE' CHECK (
        status IN ('ACTIVE', 'CLOSED', 'EARLY_WITHDRAWN')
    ),
    start_date TEXT NOT NULL,
    maturity_date TEXT NOT NULL,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES users(user_id),
    FOREIGN KEY (source_account_id) REFERENCES accounts(account_id)
);

CREATE TABLE IF NOT EXISTS loans (
    loan_id TEXT PRIMARY KEY,
    user_id TEXT NOT NULL,
    amount_vnd INTEGER NOT NULL CHECK (amount_vnd > 0),
    original_amount_vnd INTEGER GENERATED ALWAYS AS (amount_vnd) VIRTUAL,
    remaining_debt_vnd INTEGER NOT NULL DEFAULT 0 CHECK (remaining_debt_vnd >= 0),
    interest_rate REAL NOT NULL CHECK (interest_rate >= 0),
    annual_interest_rate REAL GENERATED ALWAYS AS (interest_rate) VIRTUAL,
    term_months INTEGER NOT NULL CHECK (term_months > 0),
    monthly_installment_vnd INTEGER NOT NULL DEFAULT 0 CHECK (monthly_installment_vnd >= 0),
    calc_method TEXT NOT NULL CHECK (calc_method IN ('FLAT', 'DECREASING')),
    collateral_info TEXT,
    purpose TEXT NOT NULL DEFAULT '',
    monthly_income_vnd INTEGER NOT NULL DEFAULT 0 CHECK (monthly_income_vnd >= 0),
    status TEXT NOT NULL DEFAULT 'PENDING' CHECK (
        status IN ('PENDING', 'APPROVED', 'REJECTED', 'DISBURSED', 'PAID', 'CLOSED')
    ),
    created_by TEXT NOT NULL,
    approved_by TEXT,
    disbursed_account_id TEXT,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES users(user_id),
    FOREIGN KEY (disbursed_account_id) REFERENCES accounts(account_id)
);

-- Loan document metadata is stored here; document content belongs in a configured
-- private object-storage provider, not in this database.
CREATE TABLE IF NOT EXISTS loan_documents (
    document_id TEXT PRIMARY KEY,
    loan_id TEXT NOT NULL,
    user_id TEXT NOT NULL,
    document_type TEXT NOT NULL,
    file_name TEXT NOT NULL DEFAULT '',
    mime_type TEXT NOT NULL DEFAULT '',
    file_size INTEGER NOT NULL DEFAULT 0 CHECK (file_size >= 0),
    storage_key TEXT NOT NULL DEFAULT '',
    status TEXT NOT NULL DEFAULT 'MISSING' CHECK (status IN ('MISSING', 'PENDING_REVIEW', 'APPROVED', 'REJECTED')),
    review_note TEXT NOT NULL DEFAULT '',
    reviewed_by TEXT,
    submitted_at TEXT,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    UNIQUE (loan_id, document_type),
    FOREIGN KEY (loan_id) REFERENCES loans(loan_id) ON DELETE CASCADE,
    FOREIGN KEY (user_id) REFERENCES users(user_id)
);

CREATE TABLE IF NOT EXISTS notifications (
    notification_id TEXT PRIMARY KEY,
    user_id TEXT NOT NULL,
    title TEXT NOT NULL,
    body TEXT NOT NULL,
    notification_type TEXT NOT NULL DEFAULT 'SYSTEM',
    reference_id TEXT NOT NULL DEFAULT '',
    read_at TEXT,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES users(user_id)
);

CREATE TABLE IF NOT EXISTS otp_challenges (
    challenge_id TEXT PRIMARY KEY,
    code_hash TEXT NOT NULL,
    user_id TEXT NOT NULL,
    context_reference TEXT NOT NULL,
    amount_vnd INTEGER NOT NULL CHECK (amount_vnd > 0),
    recipient_account_id TEXT NOT NULL,
    expires_at INTEGER NOT NULL,
    used_at INTEGER,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES users(user_id)
);

CREATE TABLE IF NOT EXISTS loan_schedules (
    schedule_id TEXT PRIMARY KEY,
    loan_id TEXT NOT NULL,
    period_num INTEGER NOT NULL CHECK (period_num > 0),
    due_date TEXT NOT NULL,
    principal_due_vnd INTEGER NOT NULL CHECK (principal_due_vnd >= 0),
    interest_due_vnd INTEGER NOT NULL CHECK (interest_due_vnd >= 0),
    penalty_vnd INTEGER NOT NULL DEFAULT 0 CHECK (penalty_vnd >= 0),
    status TEXT NOT NULL DEFAULT 'UNPAID' CHECK (status IN ('UNPAID', 'PAID', 'OVERDUE')),
    paid_at TEXT,
    FOREIGN KEY (loan_id) REFERENCES loans(loan_id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_accounts_user_status
    ON accounts(user_id, status);
CREATE INDEX IF NOT EXISTS idx_transactions_sender_created
    ON transactions(sender_account_id, created_at);
CREATE INDEX IF NOT EXISTS idx_transactions_receiver_created
    ON transactions(receiver_account_id, created_at);
CREATE INDEX IF NOT EXISTS idx_bills_provider_customer_status
    ON bills(provider_id, customer_code, status);
CREATE INDEX IF NOT EXISTS idx_savings_user_status
    ON savings_accounts(user_id, status);
CREATE INDEX IF NOT EXISTS idx_loans_user_status
    ON loans(user_id, status);
CREATE INDEX IF NOT EXISTS idx_loan_documents_loan
    ON loan_documents(loan_id, status);
CREATE INDEX IF NOT EXISTS idx_notifications_user_created
    ON notifications(user_id, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_notifications_user_reference
    ON notifications(user_id, reference_id);
CREATE INDEX IF NOT EXISTS idx_otp_challenges_user_expiry
    ON otp_challenges(user_id, expires_at, used_at);
CREATE INDEX IF NOT EXISTS idx_loan_schedules_loan_status
    ON loan_schedules(loan_id, status);
CREATE INDEX IF NOT EXISTS idx_audit_operator
    ON audit_logs(operator_id);
CREATE INDEX IF NOT EXISTS idx_audit_created
    ON audit_logs(created_at);
CREATE INDEX IF NOT EXISTS idx_users_role_name
    ON users(role, full_name);

-- Older builds emitted notifications from balance updates. Remove that trigger
-- because notifications are now reconstructed from successful transaction rows.
DROP TRIGGER IF EXISTS trg_accounts_notify_balance_change;

-- Keep the account poller's last_tx_id in step with inserted transactions.
CREATE TRIGGER IF NOT EXISTS trg_transactions_update_account_last_tx
AFTER INSERT ON transactions
BEGIN
    UPDATE accounts
       SET last_tx_id = NEW.transaction_id
     WHERE account_id IN (NEW.sender_account_id, NEW.receiver_account_id);
END;

-- Initialize outstanding principal for newly submitted loans.
CREATE TRIGGER IF NOT EXISTS trg_loans_initialize_remaining_debt
AFTER INSERT ON loans
WHEN NEW.remaining_debt_vnd = 0
 AND NEW.status IN ('PENDING', 'APPROVED', 'DISBURSED')
BEGIN
    UPDATE loans
       SET remaining_debt_vnd = NEW.amount_vnd
     WHERE loan_id = NEW.loan_id;
END;

-- Audit history is append-only.
CREATE TRIGGER IF NOT EXISTS trg_audit_logs_no_update
BEFORE UPDATE ON audit_logs
BEGIN
    SELECT RAISE(ABORT, 'audit_logs is append-only');
END;

CREATE TRIGGER IF NOT EXISTS trg_audit_logs_no_delete
BEFORE DELETE ON audit_logs
BEGIN
    SELECT RAISE(ABORT, 'audit_logs is append-only');
END;
