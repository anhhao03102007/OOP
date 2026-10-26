/**
 * Adapts the C++ PageBridge response to the data shapes consumed by app.js.
 *
 * Desktop host contract:
 *   - expose the QWebChannel object as window.pageBridge (or window.oopbank.pageBridge)
 *   - provide getPageDataAsync(jsonRequest) and pageDataReady signal
 *   - send { success, page, data, message }; data may contain C++ DTOs or UI sink values
 *
 * Browser requests require the desktop bridge; no local sample data is supplied.
 */
const RESPONSE_TIMEOUT_MS = 15000;
const hasOwn = (value, key) =>
  value != null && Object.prototype.hasOwnProperty.call(value, key);

function parseResponse(value) {
  return typeof value === "string" ? JSON.parse(value) : value;
}

function getDefaultBridge() {
  return globalThis.pageBridge
    ?? globalThis.oopbank?.pageBridge
    ?? globalThis.oopbankBridge?.pageBridge
    ?? null;
}

function getDefaultSessionToken() {
  return globalThis.__OOPBANK_SESSION_TOKEN__ ?? "";
}

const ACCOUNT_NAMES = {
  CHECKING: "Tài khoản thanh toán",
  SAVINGS: "Tài khoản tiết kiệm",
  DEMAND_SAVINGS: "Tiết kiệm không kỳ hạn",
  TERM_SAVINGS: "Sổ tiết kiệm có kỳ hạn",
  CREDIT: "Tài khoản tín dụng",
  LOAN: "Tài khoản khoản vay",
};

function normalizeProfile(value) {
  if (!value || typeof value !== "object") return value;
  const name = value.name ?? value.fullname ?? value.full_name ?? value.fullName ?? "";
  return {
    ...value,
    name,
    email: value.email ?? "",
    phone: value.phone ?? "",
    branch: value.branch ?? "OOPBank",
  };
}

function normalizeAccounts(value, profile) {
  const list = Array.isArray(value) ? value : (Array.isArray(value?.accounts) ? value.accounts : []);
  const primaryId = profile?.primary_account_id ?? profile?.primaryAccountId ?? "";
  return list.map((account, index) => {
    const accountId = account.id ?? account.account_id ?? account.number ?? "";
    const type = account.account_type ?? account.type ?? "";
    return {
      ...account,
      id: accountId,
      name: account.name ?? ACCOUNT_NAMES[type] ?? "Tài khoản",
      number: account.number ?? account.account_id ?? accountId,
      balance: Number(account.balance ?? account.balance_vnd ?? 0),
      primary: account.primary ?? (primaryId ? accountId === primaryId : index === 0),
    };
  });
}

function formatHistoryTime(value) {
  if (!value) return "";
  const date = new Date(value);
  if (Number.isNaN(date.getTime())) return String(value);
  return date.toLocaleString("vi-VN", {
    day: "2-digit",
    month: "2-digit",
    hour: "2-digit",
    minute: "2-digit",
  });
}

function normalizeTransactions(value, accounts, profile) {
  if (Array.isArray(value) && value.every(Array.isArray)) return value;
  let rows = Array.isArray(value) ? value : value?.items ?? value?.transactions ?? [];
  if (!Array.isArray(rows)) return [];

  // DTO wrappers may contain one history result per account.
  rows = rows.flatMap((row) => Array.isArray(row?.items) ? row.items : [row]);
  const primaryId = profile?.primary_account_id ?? accounts?.find((a) => a.primary)?.id ?? "";
  const accountIds = new Set([...(primaryId ? [primaryId] : []), ...(accounts ?? []).flatMap((a) => [a.id, a.number])]);

  return rows.map((tx) => {
    if (Array.isArray(tx)) return tx;
    const sender = tx.sender_account ?? tx.sender_account_id ?? "";
    const receiver = tx.receiver_account ?? tx.receiver_account_id ?? "";
    const type = String(tx.type ?? "").toUpperCase();
    const outgoing = accountIds.has(sender);
    const incoming = accountIds.has(receiver);
    const isIncoming = ["CREDIT_REPAYMENT", "CREDIT_INTEREST"].includes(type) ? false : incoming || (!outgoing && ["DEPOSIT", "CASH_DEPOSIT", "CREDIT"].includes(type));
    const amount = Math.abs(Number(tx.amount_vnd ?? tx.amount ?? 0));
    const amountText = (isIncoming ? "+" : "-") + amount.toLocaleString("vi-VN") + " ₫";
    const labels = {
      TRANSFER: "Chuyển tiền",
      WITHDRAW: "Rút tiền",
      CASH_WITHDRAW: "Rút tiền",
      DEPOSIT: "Tiền vào",
      CASH_DEPOSIT: "Tiền vào",
      BILL_PAYMENT: "Thanh toán hóa đơn",
      PAYMENT: "Thanh toán",
      SAVINGS_OPEN: "Gửi tiết kiệm",
      LOAN_DISBURSEMENT: "Giải ngân khoản vay",
      LOAN_REPAYMENT: "Thanh toán khoản vay",
      CREDIT_SPEND: "Chi tiêu tín dụng",
      CREDIT_REPAYMENT: "Trả nợ tín dụng",
      CREDIT_INTEREST: "Lãi tín dụng",
    };
    const label = labels[type] ?? (isIncoming ? "Tiền vào" : "Giao dịch");
    let title = tx.description ?? "";
    if (!title && type === "TRANSFER") {
      title = isIncoming ? "Nhận tiền từ TK " + sender : "Chuyển đến TK " + receiver;
    }
    if (!title) title = label;
    const icon = isIncoming ? "wallet"
      : type.includes("BILL") || type === "PAYMENT" ? "card"
      : type.includes("SAVINGS") ? "wallet"
      : "send";
    return [title, label + " · " + formatHistoryTime(tx.created_at), amountText, icon];
  });
}

function normalizePageData(page, value) {
  let payload = value ?? {};
  if (payload && hasOwn(payload, "sinks")) payload = payload.sinks;
  if (payload && page && hasOwn(payload, page)) payload = payload[page];
  if (!payload || typeof payload !== "object") return {};

  const normalized = {};
  const rawProfile = payload.profile ?? payload.user;
  const profile = rawProfile ? normalizeProfile(rawProfile) : null;
  if (profile) normalized.profile = profile;

  let rawAccounts = payload.accounts;
  if (rawAccounts?.success === false) {
    throw new Error(rawAccounts.message || "Không thể tải danh sách tài khoản.");
  }
  if (!rawAccounts && (payload.account_number || payload.payment_account_number)) {
    rawAccounts = [{
      account_id: payload.account_number ?? payload.payment_account_number,
      balance_vnd: payload.balance ?? 0,
      account_type: "CHECKING",
    }];
  }
  if (rawAccounts) normalized.accounts = normalizeAccounts(rawAccounts, profile);

  if (hasOwn(payload, "transactions")) {
    const transactionValue = payload.transactions;
    if (transactionValue?.success === false) {
      throw new Error(transactionValue.message || "Không thể tải lịch sử giao dịch.");
    }
    normalized.transactions = normalizeTransactions(transactionValue, normalized.accounts, profile);
  }

  for (const key of ["messages", "loanTerms", "loanApps", "activeLoans", "savings", "people", "audit", "config", "stats"]) {
    if (hasOwn(payload, key)) normalized[key] = payload[key];
  }
  if (Array.isArray(payload.messages)) {
    normalized.messages = payload.messages.map((item) => ({
      id: item.notification_id ?? item.id,
      title: item.title ?? "Thông báo",
      preview: item.body ?? item.preview ?? "",
      content: item.body ?? item.content ?? "",
      reference: item.reference_id ?? item.reference ?? "",
      type: item.notification_type ?? "SYSTEM",
      icon: item.notification_type === "LOAN" ? "clipboard"
        : item.notification_type === "OTP" ? "shield"
        : item.notification_type === "BALANCE_CHANGE" ? "wallet"
        : item.notification_type === "TRANSACTION" ? (String(item.title || "").includes("nhận tiền") || String(item.title || "").startsWith("Tiền đã vào") ? "wallet" : "send")
        : item.notification_type === "CREDIT_BALANCE" ? "card" : "bell",
      unread: item.unread === true,
      time: item.created_at ?? item.time ?? "",
    }));
  }
  return normalized;
}

export function createData({
  sinks = {},
  onStatus = () => {},
  getBridge = getDefaultBridge,
  getSessionToken = getDefaultSessionToken,
} = {}) {
  let sessionToken = "";
  let requestQueue = Promise.resolve();

  const status = (value, page) => {
    try {
      onStatus({ status: value, page });
    } catch (error) {
      console.warn("UI data status callback failed:", error);
    }
  };

  function enqueueBridgeRequest(bridge, requestJson) {
    const request = requestQueue.then(() => new Promise((resolve, reject) => {
      const signal = bridge.pageDataReady;

      if (typeof bridge.getPageDataAsync === "function" && signal?.connect) {
        let timer;
        const receive = (response) => {
          if (timer) clearTimeout(timer);
          signal.disconnect?.(receive);
          try {
            resolve(parseResponse(response));
          } catch (error) {
            reject(error);
          }
        };

        timer = setTimeout(() => {
          signal.disconnect?.(receive);
          reject(new Error("Hết thời gian chờ PageBridge."));
        }, RESPONSE_TIMEOUT_MS);

        try {
          signal.connect(receive);
          bridge.getPageDataAsync(requestJson);
        } catch (error) {
          clearTimeout(timer);
          signal.disconnect?.(receive);
          reject(error);
        }
        return;
      }

      if (typeof bridge.getPageData === "function") {
        try {
          Promise.resolve(bridge.getPageData(requestJson))
            .then((response) => resolve(parseResponse(response)))
            .catch(reject);
        } catch (error) {
          reject(error);
        }
        return;
      }

      reject(new Error("PageBridge không có getPageDataAsync hoặc getPageData."));
    }));

    requestQueue = request.catch(() => undefined);
    return request;
  }

  function applyResponse(response, requestedPage) {
    if (!response || response.success === false) {
      throw new Error(response?.message || "Backend không trả dữ liệu thành công.");
    }
    if (response.page !== requestedPage) {
      throw new Error("Backend trả dữ liệu không đúng trang được yêu cầu.");
    }

    const normalized = normalizePageData(response.page, response.data);
    const requiredData = {
      home: ["profile", "accounts", "transactions", "messages"],
      accounts: ["profile", "accounts", "messages"],
      transactions: ["profile", "accounts", "transactions", "messages"],
      "user-settings": ["profile", "messages"],
      notifications: ["profile", "messages"],
      savings: ["profile", "accounts", "savings", "messages"],
      services: ["profile", "accounts", "messages"],
      transfer: ["profile", "accounts", "messages"],
      topup: ["profile", "accounts", "messages"],
      payment: ["profile", "accounts", "messages"],
      "apply-loan": ["profile", "accounts", "loanApps", "activeLoans", "loanTerms", "messages"],
      loan: ["profile", "accounts", "loanApps", "activeLoans", "loanTerms", "messages"],
      "loan-pay": ["profile", "accounts", "loanApps", "activeLoans", "loanTerms", "messages"],
    }[requestedPage];
    if (!requiredData || requiredData.some((key) => !hasOwn(normalized, key))) {
      throw new Error("Backend trả thiếu dữ liệu bắt buộc cho trang này.");
    }

    let changed = false;
    for (const [name, sink] of Object.entries(sinks)) {
      if (hasOwn(normalized, name) && typeof sink === "function") {
        sink(normalized[name]);
        changed = true;
      }
    }
    return changed;
  }

  return {
    setSessionToken(token) {
      sessionToken = typeof token === "string" ? token : "";
    },

    async load(page) {
      const pageKey = String(page ?? "");
      status("loading", pageKey);

      try {
        const bridge = getBridge();
        if (!bridge) {
          return {
            changed: false,
            errors: [{ query: pageKey, message: "Không tìm thấy PageBridge. Hãy mở ứng dụng qua desktop host." }],
          };
        }

        const token = sessionToken || getSessionToken() || "";
        if (!token) {
          return {
            changed: false,
            errors: [{ query: pageKey, message: "Chưa có session token để tải dữ liệu." }],
          };
        }

        const requestJson = JSON.stringify({
          page: pageKey,
          session_token: token,
        });
        const response = await enqueueBridgeRequest(bridge, requestJson);
        return { changed: applyResponse(response, pageKey), errors: [] };
      } catch (error) {
        return {
          changed: false,
          errors: [{ query: pageKey, message: error?.message || String(error) }],
        };
      } finally {
        status("idle", pageKey);
      }
    },

    async mutate(action, payload = {}) {
      const bridge = getBridge();
      if (!bridge) return { success: false, message: "Không tìm thấy PageBridge. Hãy mở ứng dụng qua desktop host." };
      if (typeof bridge.mutate !== "function") {
        return { success: false, message: "Backend chưa cung cấp lệnh cập nhật dữ liệu." };
      }

      const token = sessionToken || getSessionToken() || "";
      const request = {
        action: String(action),
        payload,
        session_token: token,
      };
      return bridge.mutate(JSON.stringify(request));
    },
  };
}
