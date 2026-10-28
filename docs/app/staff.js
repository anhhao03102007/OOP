// Giao diện Teller (GDV) & Admin — dùng chung palette/ class với giao diện khách hàng.
export function createStaff({ state, icons, icon, brand, render, loans: L, callBackend, uploadCreditDocument, hasBackend = () => false, onLogout = () => {} }) {
  Object.assign(icons, {
    users: '<circle cx="9" cy="8" r="3.5"/><path d="M2 21a7 7 0 0 1 14 0"/><path d="M16 4.5a3.5 3.5 0 0 1 0 7M22 21a7 7 0 0 0-4-6.3"/>',
    chart: '<path d="M4 20V4M4 20h16"/><path d="M8 16v-5M12 16V8M16 16v-8"/>',
    clipboard: '<rect x="5" y="4" width="14" height="17" rx="2"/><path d="M9 4h6v3H9zM9 12h6M9 16h4"/>',
    list: '<path d="M8 6h13M8 12h13M8 18h13M3 6h.01M3 12h.01M3 18h.01"/>',
    settings: '<circle cx="12" cy="12" r="3"/><path d="M12 2v3M12 19v3M2 12h3M19 12h3M5 5l2 2M17 17l2 2M19 5l-2 2M7 17l-2 2"/>',
    search: '<circle cx="11" cy="11" r="7"/><path d="m21 21-4.3-4.3"/>',
  });
  const app = document.querySelector("#app");
  // Cập nhật tóm tắt khi Teller nhập (không dựng lại form để giữ con trỏ)
  const oaSync = (event) => {
    if (s.page !== "open-account" || !s.oa || state.role === "CUSTOMER") return;
    const form = event.target.closest?.("#open-account-form");
    if (!form) return;
    oaRead(form);
    if (s.oa.type === "SAVINGS") s.oa.savingsIdempotencyKey = "";
    const box = form.querySelector("#oa-sum");
    if (box) box.innerHTML = oaSumHtml(s.oa, oaPerson());
  };
  app.addEventListener("input", oaSync);
  app.addEventListener("change", oaSync);
  const s = {
    page: "dashboard",
    q: "",
    openAccountTarget: null,
    oa: null, // trạng thái trang mở tài khoản
    detail: null, // mã khoản vay đang xem chi tiết
    mode: null, // null | "reject" | "info"
    stats: { today_transactions: null, today_served_customers: null, total_users: null, total_customers: null, total_transactions: null, total_balance_vnd: null, pending_loans: null, locked_users: null },
    configLoaded: false,
    docView: null, // mã giấy tờ đang xem chi tiết
    creditApplications: [],
    preDoc: null, // giấy tờ chọn sẵn khi yêu cầu nộp lại
    counterReceipt: null,
    auditLookup: null,
    auditLookupQuery: "",
    auditLookupError: "",
    fees: { interbank: 0, atm: 0, rate: 0, counterLimit: 0 },
    creditConfig: { monthlyRate: 0, dueDay: 1, minimumPercent: 0 },
    staffMembers: [],
    people: [],
    audit: [],
  };
  const NAV = {
    TELLER: [["dashboard", "home", "Tổng quan"], ["customers", "users", "Khách hàng"], ["counter", "wallet", "Giao dịch quầy"], ["loans", "clipboard", "Duyệt vay"], ["credit", "card", "Hạn mức tín dụng"]],
    ADMIN: [["dashboard", "chart", "Tổng quan"], ["users", "users", "Người dùng"], ["staff", "users", "Nhân viên"], ["loans", "clipboard", "Duyệt vay"], ["credit", "card", "Hạn mức tín dụng"], ["audit", "list", "Nhật ký"], ["settings", "settings", "Cấu hình"]],
  };
  const esc = (v) => String(v).replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));
  const money = (n) => Number(n).toLocaleString("vi-VN");
  const counterExecutionId = () => {
    const bytes = new Uint8Array(24);
    if (globalThis.crypto?.getRandomValues) globalThis.crypto.getRandomValues(bytes);
    else bytes.forEach((_, index) => { bytes[index] = Math.floor(Math.random() * 256); });
    return `CASH_${Array.from(bytes, (value) => value % 10).join("")}`;
  };
  const counterPendingKey = () => `oopbank.counter.pending.${state.userId || state.role || "staff"}`;
  const readCounterPending = () => {
    try {
      const value = sessionStorage.getItem(counterPendingKey());
      return value ? JSON.parse(value) : null;
    } catch (_) { return null; }
  };
  const writeCounterPending = (value) => {
    try {
      if (value) sessionStorage.setItem(counterPendingKey(), JSON.stringify(value));
      else sessionStorage.removeItem(counterPendingKey());
    } catch (_) {}
  };
  const numericIdempotencyKey = (prefix) =>
    `${prefix}_${Date.now()}${Array.from({ length: 20 }, () => Math.floor(Math.random() * 10)).join("")}`;
  const accountTypeNames = {
    CHECKING: "Tài khoản thanh toán",
    CREDIT: "Tài khoản tín dụng",
    SAVINGS: "Sổ tiết kiệm",
    LOAN: "Khoản vay",
  };
  const balOf = (p) => Number(p.bal || 0);
  const setBal = (p, v) => { p.bal = v; };
  const hasAcc = (p) => p.role === "CUSTOMER" && p.acc !== "—";
  // Tổng số dư hệ thống = tổng số dư tất cả tài khoản khách hàng (gồm cả tài khoản đang khóa)
  const totalBalance = () => s.people.filter(hasAcc).reduce((sum, p) => sum + balOf(p), 0);
  const compact = (n) => (n >= 1e9 ? `${(n / 1e9).toFixed(1).replace(".", ",")} tỷ ₫` : n >= 1e6 ? `${(n / 1e6).toFixed(1).replace(".", ",")} triệu ₫` : `${money(n)} ₫`);
  const label = () => (state.role === "ADMIN" ? "Quản trị viên" : "Giao dịch viên");
  const toast = (msg) => {
    const el = document.createElement("div");
    el.className = "toast";
    el.textContent = msg;
    document.body.append(el);
    setTimeout(() => el.remove(), 2400);
  };
  async function loadCustomers(query = s.q) {
    s.people.splice(0, s.people.length);
    render();
    const result = await callBackend("userBridge", "searchCustomers", { session_token: state.sessionToken, query });
    const users = await Promise.all((result.customers || []).map(async (user) => {
      const accountResult = await callBackend("accountBridge", "getAccounts", { session_token: state.sessionToken, target_user_id: user.user_id });
      const account = (accountResult.accounts || []).find((item) => item.account_type === "CHECKING" && item.status === "ACTIVE");
      return { id: user.user_id, name: user.fullname, phone: user.phone, role: user.role, locked: user.status === "BLOCKED", acc: account?.account_id || "—", bal: Number(account?.balance_vnd || 0) };
    }));
    s.people.splice(0, s.people.length, ...users);
    render();
  }

  async function loadAuditLogs() {
    if (state.role !== "ADMIN") return;
    s.audit.splice(0, s.audit.length);
    render();
    const result = await callBackend("auditBridge", "getAuditLogs", { session_token: state.sessionToken, page: 1, page_size: 50 });
    s.audit = (result.items || []).map((item) => {
      const created = String(item.created_at || "");
      const date = new Date(created.replace(" ", "T"));
      const at = Number.isNaN(date.valueOf()) ? created : date.toLocaleTimeString("vi-VN", { hour: "2-digit", minute: "2-digit" });
      const detail = [item.log_id, item.action, item.target_id, item.details].filter(Boolean).join(" · ");
      return [at, item.operator_id || "Hệ thống", detail];
    });
    render();
  }

  async function loadSystemConfig() {
    s.configLoaded = false;
    const config = await callBackend("staffBridge", "getSystemConfig", { session_token: state.sessionToken });
    s.fees = {
      interbank: Number(config.interbank_fee_vnd),
      atm: Number(config.atm_fee_vnd),
      rate: Number(config.savings_max_rate_percent),
      counterLimit: Number(config.counter_max_transaction_vnd || 500000000),
    };
    s.creditConfig = {
      monthlyRate: Number(config.credit_monthly_rate_percent),
      dueDay: Number(config.credit_due_day),
      minimumPercent: Number(config.credit_min_payment_percent),
    };
    if (Array.isArray(config.loan_terms) && config.loan_terms.length) {
      L.terms.splice(0, L.terms.length, ...config.loan_terms.map((term) => ({
        months: Number(term.months), enabled: Boolean(term.enabled), rate: Number(term.rate),
      })));
    }
    s.configLoaded = true;
    render();
  }

  async function saveSystemConfig(fees, terms) {
    await callBackend("staffBridge", "updateSystemConfig", {
      session_token: state.sessionToken,
      interbank_fee_vnd: Number(fees.interbank),
      atm_fee_vnd: Number(fees.atm),
      counter_max_transaction_vnd: Number(fees.counterLimit),
      savings_max_rate_percent: Number(fees.rate),
      credit_monthly_rate_percent: Number(s.creditConfig.monthlyRate),
      credit_due_day: Number(s.creditConfig.dueDay),
      credit_min_payment_percent: Number(s.creditConfig.minimumPercent),
      loan_terms: terms,
    });
    s.fees = { ...fees };
    L.terms.splice(0, L.terms.length, ...terms.map((term) => ({ ...term })));
    await loadAuditLogs();
  }

  async function loadStaffMembers() {
    s.staffMembers.splice(0, s.staffMembers.length);
    render();
    const result = await callBackend("staffBridge", "getStaffMembers", { session_token: state.sessionToken });
    s.staffMembers.splice(0, s.staffMembers.length, ...(result.staff || []));
    render();
  }

  async function loadDashboardStats() {
    s.stats = await callBackend("staffBridge", "getDashboardStats", { session_token: state.sessionToken });
    render();
  }

  async function loadLoanApplications() {
    L.apps.splice(0, L.apps.length);
    render();
    const result = await callBackend("loanBridge", "getApplications", { session_token: state.sessionToken });
    const applications = await Promise.all((result.applications || []).map(async (row) => {
      const status = String(row.status || "PENDING").toUpperCase();
      const stage = status === "REJECTED" ? -1
        : status === "DISBURSED" ? 4
        : status === "PAID" || status === "CLOSED" ? 5
        : status === "APPROVED" ? 3 : 0;
      const documentsResult = await callBackend("loanBridge", "getDocuments", {
        session_token: state.sessionToken, loan_id: row.loan_id,
      });
      const docNames = { cccd: "CCCD gắn chip (2 mặt)", income: "Sao kê lương / HĐLĐ",
        collateral: "Giấy tờ tài sản đảm bảo", address: "Xác nhận nơi cư trú" };
      const docs = (documentsResult.documents || []).map((doc) => ({
        id: doc.document_type, documentId: doc.document_id, icon: "file",
        name: doc.document_type.startsWith("CUSTOM:") ? doc.document_type.slice(7) : (docNames[doc.document_type] || doc.document_type),
        status: doc.status === "APPROVED" ? "DONE" : doc.status === "PENDING_REVIEW" ? "REVIEW" : doc.status === "REJECTED" ? "RESUBMIT" : "MISSING",
        hint: doc.review_note || "Chưa có tệp", file: doc.status === "MISSING" ? "" : doc.file_name, mime: doc.mime_type,
        size: doc.file_size, at: doc.submitted_at, url: "",
      }));
      return {
        id: row.loan_id,
        userId: row.user_id,
        who: row.customer_name || row.user_id,
        phone: row.customer_phone || "Chưa khai báo",
        type: "Vay cá nhân",
        amt: Number(row.amount_vnd || 0),
        term: Number(row.term_months || 0),
        rate: Number(row.interest_rate || 0),
        method: row.calc_method || "DECREASING",
        purpose: row.purpose || "Chưa khai báo",
        collateral: row.collateral_info || "Chưa khai báo",
        income: Number(row.monthly_income_vnd || 0).toLocaleString("vi-VN") + " ₫/tháng",
        status,
        createdAt: row.created_at || "",
        stage,
        needInfo: false,
        resubmitted: false,
        result: status === "PENDING" ? null : { type: status, note: `Trạng thái hồ sơ: ${status}`, by: "Database", time: row.created_at || "" },
        docs,
      };
    }));
    L.apps.splice(0, L.apps.length, ...applications);
    render();
  }

  async function loadCreditApplications() {
    s.creditApplications = [];
    render();
    const result = await callBackend("creditBridge", "getApplications", { session_token: state.sessionToken });
    s.creditApplications = result.applications || [];
    render();
  }

  function creditPage() {
    const rows = s.creditApplications.map((item) => {
      const income = Number(item.monthly_income_vnd || 0);
      const maxByIncome = Math.min(500000000, income * 6);
      const maxApproval = Math.min(maxByIncome, Number(item.requested_limit_vnd));
      const documentStatus = ({ PENDING_REVIEW: "Đang kiểm tra giấy tờ", APPROVED: "Đã xác nhận giấy tờ", REJECTED: "Chờ khách nộp lại" })[item.document_status] || "Thiếu giấy tờ";
      const documentLink = item.document_storage_key
        ? `<a href="${esc(item.document_storage_key)}" target="_blank" rel="noopener">${esc(item.document_file_name || "Mở giấy tờ")}</a>` : "Chưa có tệp";
      const documentReview = item.status === "PENDING" && item.document_status === "PENDING_REVIEW"
        ? `<div class="loan-actions"><button class="btn-sm" data-sact="credit-doc-review" data-id="${esc(item.application_id)}" data-document-id="${esc(item.document_id)}" data-action="APPROVE">Xác nhận giấy tờ</button><button class="btn-sm" data-sact="credit-doc-review" data-id="${esc(item.application_id)}" data-document-id="${esc(item.document_id)}" data-action="REQUEST_RESUBMISSION">Yêu cầu nộp lại</button></div>` : "";
      const decision = item.status === "PENDING" && state.role === "ADMIN"
        ? (item.document_status === "APPROVED" && income >= 1000000
          ? `<form data-credit-decision style="display:flex;gap:.4rem;align-items:center"><input type="hidden" name="application_id" value="${esc(item.application_id)}"><input name="limit" type="number" min="1000000" max="${maxApproval}" step="1000000" value="${maxApproval}" aria-label="Hạn mức duyệt"><button class="btn-sm" name="action" value="APPROVE">Duyệt hạn mức</button><button class="btn-sm" name="action" value="REJECT">Từ chối</button></form>`
          : `<span class="product-note">Chỉ duyệt hạn mức sau khi giấy tờ thu nhập được xác nhận.</span><form data-credit-decision><input type="hidden" name="application_id" value="${esc(item.application_id)}"><button class="btn-sm" name="action" value="REJECT">Từ chối hồ sơ</button></form>`)
        : "";
      return `<div class="row"><span class="icon-box">${icon("card")}</span><div class="info"><p>${esc(item.user_id)} · Đề nghị ${money(item.requested_limit_vnd)} ₫</p><small>${esc(item.application_id)} · Thu nhập khai báo ${money(income)} ₫/tháng · Trần theo quy tắc ${money(maxByIncome)} ₫ · ${esc(item.created_at || "")}${item.decision_note ? ` · ${esc(item.decision_note)}` : ""}</small><small>Giấy tờ: ${documentStatus} · ${documentLink}${item.document_review_note ? ` · Lý do: ${esc(item.document_review_note)}` : ""}</small></div>${chip(item.status, item.status === "APPROVED" ? "ok" : item.status === "REJECTED" ? "warn" : "role")}${documentReview}${decision}</div>`;
    }).join("");
    return `${head("Hạn mức quay vòng", "Tín dụng")}<section class="surface">${rows || '<p class="product-note">Chưa có hồ sơ xin hạn mức.</p>'}</section>`;
  }
  const chip = (text, cls) => `<span class="chip ${cls}">${text}</span>`;
  const stat = (name, value, ic) => `<div class="stat"><span class="icon-box">${icon(ic)}</span><div><small>${name}</small><b>${value}</b></div></div>`;
  const fld = (id, name, text, ph, type = "text", val = "", attributes = "") =>
    `<div class="field"><label for="${id}">${text}</label><div class="input-wrap"><input id="${id}" name="${name}" type="${type}" placeholder="${ph}" value="${esc(val)}" required ${attributes} /></div></div>`;
  const head = (eyebrow, title) => `<div class="welcome"><div><p>${eyebrow}</p><h1 class="display">${title}</h1></div></div>`;
  const auditRows = (n) =>
    s.audit.length
      ? s.audit.slice(0, n).map(([t, who, msg]) => `<div class="row"><span class="icon-box">${icon("list")}</span><div class="info"><p>${esc(msg)}</p><small>${esc(who)}</small></div><small>${esc(t)}</small></div>`).join("")
      : '<p class="product-note">Chưa có nhật ký hoạt động.</p>';

  function dashboard() {
    const admin = state.role === "ADMIN";
    const pend = L.apps.filter((a) => a.stage >= 0 && a.stage < 3).length;
    const locked = s.people.filter((p) => p.locked).length;
    const stats = admin
      ? [["Khách hàng", s.stats.total_customers ?? (hasBackend() ? "—" : s.people.length), "users"],
         ["Tổng số dư hệ thống", s.stats.total_balance_vnd == null ? (hasBackend() ? "—" : compact(totalBalance())) : compact(s.stats.total_balance_vnd), "wallet"],
         ["Khoản vay chờ duyệt", s.stats.pending_loans ?? (hasBackend() ? "—" : pend), "clipboard"],
         ["Khách hàng bị khóa", s.stats.locked_users ?? (hasBackend() ? "—" : locked), "lock"]]
      : [["Giao dịch hôm nay", s.stats.today_transactions ?? "—", "send"],
         ["Hồ sơ vay chờ duyệt", s.stats.pending_loans ?? (hasBackend() ? "—" : pend), "clipboard"],
         ["Khách hàng bị khóa", s.stats.locked_users ?? (hasBackend() ? "—" : locked), "lock"],
         ["Khách phục vụ hôm nay", s.stats.today_served_customers ?? "—", "users"]];
    const quick = admin
      ? [["users", hasBackend() ? "Quản lý khách hàng" : "Quản lý người dùng"], ["staff", "Quản lý nhân viên"], ["loans", "Duyệt khoản vay"], ["settings", "Cấu hình phí & lãi suất"]]
      : [["customers", "Tra cứu khách hàng"], ["counter", "Giao dịch tại quầy"], ["loans", "Duyệt khoản vay"]];
    const today = new Date().toLocaleDateString("vi-VN", { weekday: "long", day: "2-digit", month: "long", year: "numeric" });
    return `
      <div class="welcome"><div><p>${admin ? "Bảng điều khiển hệ thống" : "Quầy giao dịch"}</p><h1 class="display">${label()}</h1></div><time>${today}</time></div>
      <div class="stat-grid">${stats.map((x) => stat(...x)).join("")}</div>
      <div class="content-grid">
        <section class="surface"><div class="surface-title"><h2 class="display">Hoạt động gần đây</h2></div>${hasBackend() && !admin ? '<p class="product-note">Nhật ký hoạt động chỉ dành cho Admin.</p>' : auditRows(4)}</section>
        <section class="surface"><div class="surface-title"><h2 class="display">Tác vụ nhanh</h2></div>
          <div class="quick-list">${quick.map(([p, t]) => `<button class="service-item" data-spage="${p}"><b>${t}</b>${icon("arrow")}</button>`).join("")}</div>
        </section>
      </div>`;
  }

  function peoplePage(admin) {
    const q = s.q.toLowerCase();
    const roleName = { CUSTOMER: "Khách hàng", TELLER: "Giao dịch viên", ADMIN: "Quản trị" };
    const list = s.people.filter((p) => (admin || p.role === "CUSTOMER") && `${p.name} ${p.phone} ${p.acc} ${p.id}`.toLowerCase().includes(q));
    const rows = list.map((p) => `
      <div class="row"><span class="icon-box">${icon("user")}</span>
        <div class="info"><p>${p.name}</p><small>${p.phone} · ${p.acc}${hasAcc(p) ? ` · ${money(balOf(p))} ₫` : ""}</small></div>
        ${admin ? chip(roleName[p.role], "role") : ""}${chip(p.locked ? "Đã khóa" : "Hoạt động", p.locked ? "warn" : "ok")}
        <div class="row-actions">
          ${!admin ? `<button class="btn-sm" data-sact="open-acc" data-id="${p.id}">Mở tài khoản</button>` : ""}
          <button class="btn-sm ${p.locked ? "" : "danger"}" data-sact="toggle-lock" data-id="${p.id}" ${p.role === "ADMIN" ? "disabled" : ""}>${p.locked ? "Mở khóa" : "Khóa"}</button>
        </div>
      </div>`).join("");
    return `${head(admin ? (hasBackend() ? "Quản lý khách hàng" : "Quản trị người dùng") : "Tra cứu khách hàng", admin && !hasBackend() ? "Người dùng" : "Khách hàng")}
      <form id="staff-search" class="search-row"><div class="input-wrap">${icon("search")}<input name="q" value="${esc(s.q)}" placeholder="Tên, số điện thoại, số tài khoản..." /></div><button class="primary" type="submit">Tìm kiếm</button></form>
      <section class="surface">${rows || '<p class="product-note">Không tìm thấy kết quả.</p>'}</section>`;
  }

  // ===== Teller mở tài khoản thanh toán tại quầy =====
  const OA_PRODUCTS = {
    CHECKING: { name: "Tài khoản thanh toán", sub: "Nhận, chuyển và rút tiền hằng ngày", icon: "wallet" },
    CREDIT: { name: "Hồ sơ tín dụng", sub: "Xin hạn mức tín dụng chờ Admin phê duyệt", icon: "card" },
    SAVINGS: { name: "Sổ tiết kiệm", sub: "Gửi tiền từ tài khoản thanh toán", icon: "wallet" },
    LOAN: { name: "Hồ sơ vay", sub: "Lập đơn vay và ghi nhận hồ sơ tại quầy", icon: "clipboard" },
  };
  const oaNew = () => ({
    type: "CHECKING",
    kyc: false,
    v: {
      CHECKING: { init: 0 },
      CREDIT: { limit: 10000000, income: 5000000, pinValid: false, file: null },
      SAVINGS: { amount: 1000000, term: "M:6", maturity: "CLOSE_TO_ACCOUNT", funding: "ACCOUNT", pinValid: false },
      LOAN: { amount: 10000000, term: 12, calc: "DECREASING", purpose: "", income: 0, collateral: "", pinValid: false, docs: {} },
    },
  });
  const oaPerson = () => s.people.find((p) => p.id === s.openAccountTarget && p.role === "CUSTOMER");

  // Đọc giá trị từ form vào s.oa (gọi mỗi khi người dùng nhập)
  function oaRead(form) {
    const o = s.oa;
    if (!o) return;
    const d = new FormData(form);
    const v = o.v[o.type];
    const n = (key) => d.get(key) === null || d.get(key) === "" ? 0 : Number(d.get(key));
    o.kyc = d.get("kyc") !== null;
    if (o.type === "CHECKING") v.init = n("init");
    if (o.type === "SAVINGS") {
      v.amount = n("amount");
      v.term = String(d.get("term_months") || "");
      v.maturity = String(d.get("maturity_option") || "");
      v.funding = String(d.get("funding_method") || "ACCOUNT");
      v.pinValid = /^\d{6}$/.test(String(d.get("pin") || ""));
    }
    if (o.type === "LOAN") {
      v.amount = n("amount");
      v.term = n("term_months");
      v.calc = String(d.get("calc_method") || "DECREASING");
      v.purpose = String(d.get("purpose") || "").trim();
      v.income = n("monthly_income_vnd");
      v.collateral = String(d.get("collateral_info") || "").trim();
      v.pinValid = /^\d{6}$/.test(String(d.get("customer_pin") || ""));
      v.docs = Object.fromEntries(["cccd", "income", "collateral", "address"].map((doc) => [doc, d.get(`doc_${doc}`) !== null]));
    }
    if (o.type === "CREDIT") {
      v.limit = n("requested_limit_vnd");
      v.income = n("monthly_income_vnd");
      v.pinValid = /^\d{6}$/.test(String(d.get("customer_pin") || ""));
      v.file = d.get("income_document");
    }
  }

  function oaError(o, person) {
    const v = o.v[o.type];
    if (!person) return "Không tìm thấy khách hàng.";
    if (person.locked) return "Tài khoản của khách đang bị khóa.";
    if (!o.kyc) return "Cần đối chiếu CCCD bản gốc trước.";
    if (o.type === "CHECKING" && (!Number.isSafeInteger(v.init) || v.init < 0 || v.init > 500000000)) return "Tiền nạp ban đầu phải là số nguyên từ 0 đến 500.000.000 ₫.";
    if (o.type === "SAVINGS") {
      if (!person.acc || person.acc === "—") return "Khách hàng cần có tài khoản thanh toán đang hoạt động để mở sổ.";
      if (!Number.isSafeInteger(v.amount) || v.amount < 1000000) return "Số tiền gửi phải là số nguyên từ 1.000.000 ₫ trở lên.";
      if (v.funding === "ACCOUNT" && v.amount > person.bal) return "Số tiền gửi vượt số dư tài khoản thanh toán.";
      if (!savingsTerms.some((term) => term.value === v.term)) return "Kỳ hạn tiết kiệm không hợp lệ.";
      if (!["RENEW_ALL", "RENEW_PRINCIPAL", "CLOSE_TO_ACCOUNT"].includes(v.maturity)) return "Chọn phương thức đáo hạn.";
      if (!v.pinValid) return "Cần nhập PIN giao dịch 6 số của khách hàng.";
    }
    if (o.type === "LOAN") {
      const configuredTerm = L.terms.find((term) => term.months === v.term && term.enabled);
      if (!Number.isSafeInteger(v.amount) || v.amount <= 0) return "Số tiền vay phải là số nguyên dương.";
      if (!configuredTerm) return "Kỳ hạn vay không được mở theo cấu hình hiện tại.";
      if (!Number.isSafeInteger(v.income) || v.income <= 0 || v.income > 1000000000000) return "Thu nhập hàng tháng phải là số nguyên dương hợp lệ.";
      if (!v.purpose || v.purpose.length > 300) return "Mục đích vay không được để trống và tối đa 300 ký tự.";
      if (!["FLAT", "DECREASING"].includes(v.calc)) return "Phương thức tính lãi không hợp lệ.";
      if (!v.pinValid) return "Cần nhập PIN giao dịch 6 số của khách hàng.";
    }
    if (o.type === "CREDIT") {
      const maxLimit = Math.min(500000000, v.income * 6);
      if (!Number.isSafeInteger(v.income) || v.income < 1000000 || v.income > 10000000000) return "Thu nhập tháng phải từ 1.000.000 đến 10.000.000.000 đồng.";
      if (!Number.isSafeInteger(v.limit) || v.limit < 1000000 || v.limit > maxLimit) return "Hạn mức đề nghị không được vượt quá 6 lần thu nhập tháng hoặc 500.000.000 đồng.";
      if (!(v.file instanceof File) || v.file.size <= 0 || v.file.size > 5 * 1024 * 1024 || !["application/pdf", "image/jpeg", "image/png"].includes(v.file.type)) return "Cần chọn giấy tờ chứng minh thu nhập dạng PDF/JPG/PNG, tối đa 5 MB.";
      if (!v.pinValid) return "Cần nhập PIN giao dịch 6 số của khách hàng.";
    }
    return "";
  }

  function oaSummary(o, person) {
    const v = o.v[o.type];
    const x = { title: OA_PRODUCTS[o.type].name, rows: [], status: ["Mở ngay", "ok"], submit: "Xác nhận mở tài khoản" };
    if (o.type === "CHECKING") {
      x.bigLabel = "Nạp ban đầu";
      x.big = `${money(v.init)} ₫`;
      x.rows = [["Số tài khoản", "Hệ thống cấp"], ["Tài khoản chính", "Tự chọn theo tài khoản thanh toán tạo sớm nhất"]];
    } else if (o.type === "CREDIT") {
      x.bigLabel = "Hạn mức đề nghị";
      x.big = `${money(v.limit)} ₫`;
      x.rows = [["Thu nhập hàng tháng", `${money(v.income)} ₫`], ["Giấy tờ chứng minh thu nhập", v.file?.name || "Chưa chọn"], ["Trạng thái", "Chờ Admin phê duyệt"]];
      x.status = ["Cần PIN và giấy tờ", "warn"];
      x.submit = "Nộp hồ sơ tín dụng";
    } else if (o.type === "SAVINGS") {
      x.bigLabel = "Số tiền gửi";
      x.big = `${money(v.amount)} ₫`;
      const selectedTerm = savingsTerms.find((term) => term.value === v.term);
      x.rows = [["Kỳ hạn", selectedTerm ? `${selectedTerm.label} · ${selectedTerm.rate}%/năm` : "—"], ["Nguồn tiền", v.funding === "COUNTER_CASH" ? "Tiền mặt tại quầy" : "Trích tài khoản"], ["Tài khoản nhận đáo hạn", person?.acc || "—"], ["Đáo hạn", ({ RENEW_ALL: "Tái tục gốc và lãi", RENEW_PRINCIPAL: "Tái tục gốc", CLOSE_TO_ACCOUNT: "Chuyển về tài khoản" })[v.maturity] || "—"]];
      x.status = ["Cần PIN khách hàng", "warn"];
      x.submit = "Xác nhận mở sổ tiết kiệm";
    } else {
      x.bigLabel = "Số tiền vay";
      x.big = `${money(v.amount)} ₫`;
      x.rows = [["Kỳ hạn", `${v.term} tháng`], ["Thu nhập hàng tháng", `${money(v.income)} ₫`], ["Giấy tờ đã nhận", `${Object.values(v.docs).filter(Boolean).length}/4`], ["Trạng thái", "Chờ kiểm tra và thẩm định"]];
      x.status = ["Cần PIN khách hàng", "warn"];
      x.submit = "Nộp hồ sơ vay";
    }
    return x;
  }

  function oaSumHtml(o, person) {
    const x = oaSummary(o, person);
    const error = oaError(o, person);
    return `<h3 class="display">${x.title}</h3>
      <small>${x.bigLabel}</small><p class="oa-big display">${x.big}</p>
      <dl>${x.rows.map(([k, val]) => `<div><dt>${k}</dt><dd>${esc(val)}</dd></div>`).join("")}</dl>
      ${chip(esc(x.status[0]), x.status[1])}
      <button class="primary" type="submit" ${error ? "disabled" : ""}>${x.submit}</button>
      <p class="product-note oa-hint" aria-live="polite">${esc(error)}</p>`;
  }

  const oaNum = (name, text, val, unit, cls = "") =>
    `<div class="field ${cls}"><label for="oa-${name}">${text}</label><div class="input-wrap"><input id="oa-${name}" name="${name}" type="number" min="0" step="100000" inputmode="numeric" placeholder="0" value="${val || ""}" /><span class="oa-unit">${unit}</span></div></div>`;
  const oaOpt = (name, value, text, checked, sub = "") =>
    `<label class="oa-opt"><input type="radio" name="${name}" value="${value}" ${checked ? "checked" : ""} /><span>${text}${sub ? `<small>${sub}</small>` : ""}</span></label>`;
  const savingsTerms = [
    ...[1, 2, 3].map((weeks) => ({ value: `W:${weeks}`, label: `${weeks} tuần`, months: 1, days: weeks * 7, rate: 0.5 })),
    ...[
      [1, 4.5], [2, 4.6], [3, 4.7], [4, 4.7], [5, 4.7],
      [6, 5.3], [7, 5.3], [8, 5.3], [9, 5.3], [10, 5.3], [11, 5.3],
      [13, 5.85], [15, 5.85], [18, 5.85], [24, 6.5], [36, 6.5], [48, 6.5], [60, 6.5],
    ].map(([months, rate]) => ({ value: `M:${months}`, label: `${months} tháng`, months, days: 0, rate })),
  ];

  function oaForm(o, person) {
    const v = o.v[o.type];
    if (o.type === "CHECKING") {
      return `<h2 class="display">Tài khoản thanh toán</h2>
        <p class="product-note">Số tài khoản do hệ thống cấp. Giao dịch viên xác nhận đã đối chiếu CCCD; khách tự đặt PIN sau khi đăng nhập.</p>
        <div class="oa-grid">
          ${oaNum("init", "Nạp tiền mặt ban đầu (không bắt buộc)", v.init, "₫")}
          <p class="product-note oa-full">Tài khoản chính được hệ thống xác định theo tài khoản thanh toán tạo sớm nhất.</p>
        </div>`;
    }
    if (o.type === "SAVINGS") {
      const termOptions = savingsTerms.map((term) =>
        `<option value="${term.value}" ${v.term === term.value ? "selected" : ""}>${term.label} · ${term.rate}%/năm</option>`
      ).join("");
      return `<h2 class="display">Mở sổ tiết kiệm</h2>
        <p class="product-note">Sổ đứng tên khách hàng. Có thể trích từ tài khoản hoặc nhận tiền mặt khách nộp tại quầy; tiền đáo hạn chuyển về tài khoản thanh toán đã chọn.</p>
        <div class="oa-grid">
          ${oaNum("amount", "Số tiền gửi", v.amount, "₫")}
          <div class="field"><label for="oa-term_months">Kỳ hạn</label><div class="input-wrap"><select id="oa-term_months" name="term_months">${termOptions}</select></div></div>
          <div class="field oa-full"><label>Nguồn tiền gửi</label><div class="oa-opts">
            ${oaOpt("funding_method", "ACCOUNT", "Trích tài khoản", v.funding === "ACCOUNT")}
            ${oaOpt("funding_method", "COUNTER_CASH", "Tiền mặt khách nộp", v.funding === "COUNTER_CASH")}
          </div></div>
          <div class="field oa-full"><label>Phương thức khi đáo hạn</label><div class="oa-opts">
            ${oaOpt("maturity_option", "CLOSE_TO_ACCOUNT", "Chuyển về tài khoản", v.maturity === "CLOSE_TO_ACCOUNT")}
            ${oaOpt("maturity_option", "RENEW_PRINCIPAL", "Tái tục gốc", v.maturity === "RENEW_PRINCIPAL")}
            ${oaOpt("maturity_option", "RENEW_ALL", "Tái tục gốc và lãi", v.maturity === "RENEW_ALL")}
          </div></div>
          <p class="oa-source oa-full">Tài khoản thanh toán nhận tiền đáo hạn: <b>${esc(person.acc)}</b>${v.funding === "ACCOUNT" ? ` · Số dư hiện tại: <b>${money(person.bal)} ₫</b>` : ""}</p>
          <div class="field oa-full"><label for="oa-pin">PIN giao dịch của khách hàng</label><div class="input-wrap"><input id="oa-pin" name="pin" type="password" inputmode="numeric" maxlength="6" autocomplete="off" placeholder="Khách hàng tự nhập PIN 6 số" /></div></div>
        </div>`;
    }
    if (o.type === "LOAN") {
      const terms = L.terms.filter((term) => term.enabled);
      const termOptions = terms.map((term) => `<option value="${term.months}" ${v.term === term.months ? "selected" : ""}>${term.months} tháng · ${term.rate}%/năm</option>`).join("");
      const documents = [["cccd", "CCCD"], ["income", "Chứng minh thu nhập"], ["collateral", "Hồ sơ tài sản bảo đảm"], ["address", "Xác nhận địa chỉ"]];
      return `<h2 class="display">Lập hồ sơ vay</h2>
        <p class="product-note">Đơn vay được tạo cho khách hàng đang chọn. Teller đối chiếu CCCD, xác thực PIN và đánh dấu đúng những giấy tờ đã tiếp nhận; Admin thực hiện phê duyệt sau khi kiểm tra.</p>
        <div class="oa-grid">
          ${oaNum("amount", "Số tiền vay", v.amount, "₫")}
          ${oaNum("monthly_income_vnd", "Thu nhập hàng tháng", v.income, "₫")}
          <div class="field"><label for="oa-loan-term">Kỳ hạn</label><div class="input-wrap"><select id="oa-loan-term" name="term_months" required>${termOptions}</select></div></div>
          <div class="field"><label for="oa-calc-method">Phương thức tính lãi</label><div class="input-wrap"><select id="oa-calc-method" name="calc_method"><option value="DECREASING" ${v.calc === "DECREASING" ? "selected" : ""}>Dư nợ giảm dần</option><option value="FLAT" ${v.calc === "FLAT" ? "selected" : ""}>Dư nợ ban đầu</option></select></div></div>
          <div class="field oa-full"><label for="oa-purpose">Mục đích vay</label><div class="input-wrap"><input id="oa-purpose" name="purpose" maxlength="300" value="${esc(v.purpose)}" required /></div></div>
          <div class="field oa-full"><label for="oa-collateral">Thông tin tài sản bảo đảm (nếu có)</label><div class="input-wrap"><input id="oa-collateral" name="collateral_info" maxlength="300" value="${esc(v.collateral)}" /></div></div>
          <div class="field oa-full"><label>Giấy tờ đã tiếp nhận</label><div class="oa-opts">${documents.map(([doc, label]) => `<label class="oa-opt"><input type="checkbox" name="doc_${doc}" ${v.docs[doc] ? "checked" : ""} /><span>${label}</span></label>`).join("")}</div></div>
          <div class="field oa-full"><label for="oa-loan-pin">PIN giao dịch của khách hàng</label><div class="input-wrap"><input id="oa-loan-pin" name="customer_pin" type="password" inputmode="numeric" pattern="[0-9]{6}" maxlength="6" autocomplete="off" placeholder="Khách hàng tự nhập PIN 6 số" required /></div></div>
        </div>`;
    }
    if (o.type === "CREDIT") {
      return `<h2 class="display">Nộp hồ sơ tín dụng</h2>
        <p class="product-note">Teller xác thực khách hàng và tải giấy tờ chứng minh thu nhập lên kho lưu trữ. Hạn mức chỉ được tạo sau khi Admin phê duyệt.</p>
        <div class="oa-grid">
          ${oaNum("monthly_income_vnd", "Thu nhập hàng tháng", v.income, "₫")}
          ${oaNum("requested_limit_vnd", "Hạn mức đề nghị", v.limit, "₫")}
          <div class="field oa-full"><label for="oa-credit-document">Giấy tờ chứng minh thu nhập (PDF/JPG/PNG, tối đa 5 MB)</label><div class="input-wrap"><input id="oa-credit-document" name="income_document" type="file" accept=".pdf,.jpg,.jpeg,.png,application/pdf,image/jpeg,image/png" required /></div></div>
          <div class="field oa-full"><label for="oa-credit-pin">PIN giao dịch của khách hàng</label><div class="input-wrap"><input id="oa-credit-pin" name="customer_pin" type="password" inputmode="numeric" pattern="[0-9]{6}" maxlength="6" autocomplete="off" placeholder="Khách hàng tự nhập PIN 6 số" required /></div></div>
        </div>`;
    }
    return "";
  }

  function openAccountPage() {
    const o = s.oa;
    const person = oaPerson();
    if (!o || !person) {
      return `${head("Nghiệp vụ tại quầy", "Mở tài khoản tại quầy")}<section class="surface"><p class="product-note">Không tìm thấy khách hàng.</p><button class="btn-sm" type="button" data-sact="open-acc-close">← Danh sách khách hàng</button></section>`;
    }
    const picker = Object.entries(OA_PRODUCTS).map(([type, p]) =>
      `<button class="oa-pick" type="button" role="tab" aria-selected="${type === o.type}" data-sact="oa-type" data-id="${type}"><span class="icon-box">${icon(p.icon)}</span><b>${p.name}</b><small>${p.sub}</small></button>`).join("");
    return `<form id="open-account-form" novalidate>
      <button class="btn-sm oa-back" type="button" data-sact="open-acc-close">← Danh sách khách hàng</button>
      ${head("Nghiệp vụ tại quầy", "Mở tài khoản tại quầy")}
      <section class="oa-cust" aria-label="Khách hàng">
        <span class="icon-box">${icon("user")}</span>
        <div class="info"><p>${esc(person.name)} · ${esc(person.id)}</p><small>${esc(person.phone)} · ${esc(person.acc)}</small></div>
        <label class="check"><input type="checkbox" name="kyc" ${o.kyc ? "checked" : ""} /> Đã đối chiếu CCCD bản gốc</label>
        ${chip(person.locked ? "Đã khóa" : "Hoạt động", person.locked ? "warn" : "ok")}
      </section>
      <div class="oa-layout">
        <div class="oa-picker" role="tablist" aria-label="Loại sản phẩm">${picker}</div>
        <section class="form-card oa-panel">${oaForm(o, person)}</section>
        <aside class="oa-sum" id="oa-sum">${oaSumHtml(o, person)}</aside>
      </div>
    </form>`;
  }

  // Tạo sản phẩm tại quầy bằng session nhân viên đã xác thực.
  async function oaBackend(person, type, v) {
    const base = { session_token: state.sessionToken, target_user_id: person.id, kyc_verified: s.oa?.kyc === true };
    if (type === "CHECKING") {
      return callBackend("accountBridge", "openNewAccount", {
        ...base,
        account_type: type,
        initial_balance_vnd: v.init,
      });
    }
    if (type === "SAVINGS") {
      const form = document.querySelector("#open-account-form");
      const pin = String(new FormData(form).get("pin") || "");
      const savingsTerm = savingsTerms.find((term) => term.value === v.term);
      if (!savingsTerm) throw new Error("Kỳ hạn tiết kiệm không hợp lệ.");
      if (!s.oa.savingsIdempotencyKey) s.oa.savingsIdempotencyKey = numericIdempotencyKey("SAV");
      return callBackend("savingsBridge", "openSavings", {
        ...base,
        idempotency_key: s.oa.savingsIdempotencyKey,
        amount_vnd: v.amount,
        term_months: savingsTerm.months,
        term_days: savingsTerm.days,
        maturity_option: v.maturity,
        funding_method: v.funding,
        pin,
        source_account_id: person.acc,
      });
    }
    if (type === "LOAN") {
      const form = document.querySelector("#open-account-form");
      const data = new FormData(form);
      const receivedDocumentTypes = ["cccd", "income", "collateral", "address"].filter((doc) => data.get(`doc_${doc}`) !== null);
      return callBackend("loanBridge", "applyLoan", {
        ...base,
        amount_vnd: v.amount,
        term_months: v.term,
        calc_method: v.calc,
        collateral_info: v.collateral,
        purpose: v.purpose,
        monthly_income_vnd: v.income,
        customer_pin: String(data.get("customer_pin") || ""),
        received_document_types: receivedDocumentTypes,
      });
    }
    if (type === "CREDIT") {
      const form = document.querySelector("#open-account-form");
      const data = new FormData(form);
      const file = data.get("income_document");
      if (!(file instanceof File) || typeof uploadCreditDocument !== "function") throw new Error("Không thể tải giấy tờ chứng minh thu nhập.");
      const storageKey = await uploadCreditDocument(file);
      return callBackend("creditBridge", "apply", {
        ...base,
        requested_limit_vnd: v.limit,
        monthly_income_vnd: v.income,
        customer_pin: String(data.get("customer_pin") || ""),
        file_name: file.name,
        mime_type: file.type,
        file_size: file.size,
        storage_key: storageKey,
      });
    }
    throw new Error("Loại tài khoản chưa được hỗ trợ tại quầy.");
  }

  async function openAccountSubmit(form) {
    const o = s.oa;
    const person = oaPerson();
    if (!o || o.submitting) return;
    oaRead(form);
    const error = oaError(o, person);
    if (error) { toast(error); return; }
    o.submitting = true;
    const submitButton = form.querySelector('button[type="submit"]');
    if (submitButton) submitButton.disabled = true;
    const v = o.v[o.type];
    const type = o.type;
    let warning = "";
    let resultId = "";
    try {
      if (!hasBackend()) throw new Error("Chưa kết nối database. Không thể ghi nhận nghiệp vụ mở tài khoản.");
      const result = await oaBackend(person, type, v);
      warning = result.warning || "";
      resultId = result.transaction_id || result.savings_id || result.loan_id || result.created_application_id || "";
    } catch (err) {
      o.submitting = false;
      if (submitButton) submitButton.disabled = false;
      toast(`Không thực hiện được: ${err?.message || String(err)}`);
      return;
    }
    s.page = "customers";
    s.oa = null;
    s.openAccountTarget = null;
    render();
    const doneMessage = type === "SAVINGS"
      ? `Đã mở sổ tiết kiệm cho ${person.name}`
      : type === "LOAN" ? `Đã nộp hồ sơ vay cho ${person.name}`
        : type === "CREDIT" ? `Đã nộp hồ sơ tín dụng cho ${person.name}` : `Đã mở tài khoản thanh toán cho ${person.name}`;
    toast(warning || `${doneMessage}${resultId ? `. Mã: ${resultId}` : ""}`);
    if (hasBackend()) {
      try {
        await loadCustomers(s.q);
      } catch (err) {
        toast(`Đã lưu, nhưng không tải lại được dữ liệu: ${err?.message || String(err)}`);
      }
    }
  }

  function counterReceiptPanel() {
    const receipt = s.counterReceipt;
    if (!receipt) return "";
    const lookup = s.auditLookup ? `
      <div class="row"><div class="info"><p>${esc(s.auditLookup.action)} · ${esc(s.auditLookup.target_id)}</p>
        <small>${esc(s.auditLookup.operator_id)} · ${esc(s.auditLookup.created_at)}</small>
        <small>${esc(s.auditLookup.details || "")}</small></div></div>` : "";
    return `<section class="surface">
      <div class="surface-title"><h2 class="display">Kết quả gần nhất</h2>
        <span class="chip ${receipt.success ? "ok" : "warn"}">${receipt.success ? "Thành công" : "Thất bại"}</span></div>
      <p>${esc(receipt.message)}</p>
      <p><strong>Mã tra cứu:</strong> <code>${esc(receipt.executionId || "")}</code></p>
      ${receipt.transactionId ? `<p><strong>Mã giao dịch:</strong> <code>${esc(receipt.transactionId)}</code></p>` : ""}
      ${receipt.tracePersisted ? "" : '<p class="product-note">Chưa xác nhận lưu nhật ký; có thể thử tra cứu lại sau.</p>'}
      <form id="counter-lookup-form">
        ${fld("counter-lookup-id", "log_id", "Tra cứu nhật ký theo mã", "CASH_…", "text", s.auditLookupQuery || receipt.executionId)}
        <button class="secondary" type="submit">Tra cứu mã ${icon("search")}</button>
      </form>
      ${lookup}${s.auditLookupError ? `<p class="product-note">${esc(s.auditLookupError)}</p>` : ""}
    </section>`;
  }

  function counter() {
    const pending = readCounterPending();
    if (pending && !s.counterReceipt) {
      s.counterReceipt = {
        executionId: pending.executionId,
        transactionId: "",
        success: false,
        tracePersisted: false,
        message: "Chưa nhận được kết quả cuối cùng. Gửi lại đúng thông tin để hệ thống tra cứu giao dịch trước khi tạo giao dịch khác.",
      };
    }
    return `${head("Nghiệp vụ tại quầy", "Giao dịch quầy")}
      <div class="feature-layout">
        <section class="form-card"><h2 class="display">Nạp / Rút tiền mặt</h2>
          <form id="counter-form">
            <div class="form-grid">
              <div class="field"><label for="c-type">Loại giao dịch</label><div class="input-wrap"><select id="c-type" name="type"><option value="DEPOSIT">Nạp tiền</option><option value="WITHDRAW">Rút tiền</option></select></div></div>
              ${fld("c-acc", "acc", "Số tài khoản", "Nhập số tài khoản")}
            </div>
            ${fld("c-amt", "amount", "Số tiền", "0 VND", "number")}
            <div class="field"><label for="c-pin">Mã PIN giao dịch của khách</label><div class="input-wrap"><input id="c-pin" name="pin" type="password" inputmode="numeric" maxlength="6" autocomplete="off" placeholder="Chỉ khách hàng tự nhập" /></div></div>
            <button class="primary" type="submit">Xác nhận giao dịch ${icon("arrow")}</button>
          </form>
        </section>
        <aside class="source-card"><span class="eyebrow">Lưu ý quầy</span><p>Đối chiếu CCCD của khách trước khi thực hiện giao dịch.</p><hr /><p>Hạn mức mỗi giao dịch</p><strong>${money(s.fees.counterLimit)} ₫</strong><hr /><p>Phí rút ATM ngoại mạng</p><strong>${money(s.fees.atm)} ₫</strong></aside>
      </div>
      ${counterReceiptPanel()}`;
  }

  const appStatus = (a) =>
    a.stage < 0 ? ["Đã từ chối", "warn"]
    : a.status === "PAID" ? ["Đã tất toán", "ok"]
    : a.status === "CLOSED" ? ["Đã đóng", "ok"]
    : a.stage === 4 ? ["Đã giải ngân", "ok"]
    : a.stage === 3 ? ["Đã duyệt · chờ giải ngân", "ok"]
    : a.needInfo ? ["Chờ khách bổ sung", "warn"]
    : a.resubmitted ? ["Khách đã bổ sung", "role"]
    : ["Chờ duyệt", "role"];
  const rank = (a) => (a.stage < 0 ? 3 : a.stage === 4 ? 2 : a.stage === 3 ? 1 : 0);

  function loansPage() {
    const rows = [...L.apps].sort((a, b) => rank(a) - rank(b)).map((a) => {
      const [t, c] = appStatus(a);
      return `
      <div class="row"><span class="icon-box">${icon("card")}</span>
        <div class="info"><p>${esc(a.who)} · ${money(a.amt)} ₫</p><small>${a.id} · ${a.type} · ${a.term} tháng</small></div>
        ${chip(t, c)}
        <div class="row-actions"><button class="btn-sm" data-sact="loan-view" data-id="${a.id}">Xem chi tiết</button></div>
      </div>`;
    }).join("");
    return `${head("Quản lý tín dụng", "Duyệt khoản vay")}<section class="surface">${rows}</section>`;
  }

  const mimeName = (m) => ({ "image/jpeg": "Ảnh JPG", "image/png": "Ảnh PNG", "application/pdf": "Tài liệu PDF" }[m] || m || "—");

// Chi tiết một giấy tờ trong hồ sơ: thông tin tệp, xem trước, xác nhận / yêu cầu nộp lại
  function docDetail(a, d, decided) {
    const [t, c] = L.docStatus[d.status];
    const fact = (k, v) => `<div><small>${k}</small><b>${esc(v || "—")}</b></div>`;
    const preview = d.status === "MISSING"
      ? `<div class="doc-preview empty">${icon("file")}<p>Khách hàng chưa nộp giấy tờ này.</p></div>`
      : d.url && (d.mime || "").startsWith("image/")
        ? `<div class="doc-preview"><img src="${d.url}" alt="${esc(d.name)}" /></div>`
        : `<div class="doc-preview empty">${icon(d.icon)}<p>${esc(d.file)}</p><small>${d.url ? `<a href="${d.url}" target="_blank" rel="noopener">Mở tệp trong tab mới</a>` : "Bản xem trước chưa khả dụng"}</small></div>`;
    const actions = !decided && d.status === "REVIEW"
      ? `<div class="loan-actions">${state.role === "ADMIN" ? `<button class="primary" data-sact="doc-ok" data-id="${a.id}" data-doc="${d.id}">Xác nhận giấy tờ</button>` : ""}<button class="secondary" data-sact="doc-redo" data-doc="${d.id}">Yêu cầu nộp lại</button></div>`
      : !decided && (d.status === "MISSING" || d.status === "RESUBMIT") ? `<div class="loan-actions"><button class="secondary" data-sact="doc-redo" data-doc="${d.id}">${d.status === "RESUBMIT" ? "Yêu cầu nộp lại lần nữa" : "Gửi yêu cầu bổ sung"}</button></div>` : "";
    return `
      <div class="doc-head"><button class="btn-sm" data-sact="doc-back">← Quay lại hồ sơ vay</button>${chip(t, c)}</div>
      <h4 class="modal-sub">${esc(d.name)}</h4>
      ${preview}
      <div class="loan-facts">${fact("Thuộc hồ sơ", a.id)}${fact("Trạng thái", t)}${fact("Tên tệp", d.file)}${fact("Loại tệp", d.file ? mimeName(d.mime) : "")}${fact("Dung lượng", d.size)}${fact("Thời gian nộp", d.at)}</div>
      ${actions}`;
  }

  // Hộp thông tin khoản vay: xem đủ thông tin & hồ sơ trước khi phê duyệt / từ chối / yêu cầu bổ sung
  function loanModal() {
    const a = s.detail && L.apps.find((x) => x.id === s.detail);
    if (!a) return "";
    const monthly = Math.round(a.amt / a.term + (a.amt * a.rate) / 1200);
    const methodName = { DECREASING: "Dư nợ giảm dần", FIXED: "Niên kim cố định" }[a.method] || a.method;
    const missing = a.docs.filter((d) => d.status === "MISSING" || d.status === "RESUBMIT").length;
    const unverified = a.docs.filter((d) => d.status !== "DONE").length;
    const decided = a.stage >= 3 || a.stage < 0;
    const [st, sc] = appStatus(a);
    const fact = (k, v) => `<div><small>${k}</small><b>${esc(v)}</b></div>`;
    const facts = [
      fact("Khách hàng", a.who), fact("Số điện thoại", a.phone), fact("Loại vay", a.type), fact("Số tiền vay", `${money(a.amt)} ₫`),
      fact("Kỳ hạn", `${a.term} tháng`), fact("Lãi suất", `${a.rate}%/năm`), fact("Phương thức trả nợ", methodName), fact("Trả hàng tháng (ước tính)", `${money(monthly)} ₫`),
      fact("Tổng lãi (ước tính)", `${money(monthly * a.term - a.amt)} ₫`), fact("Mục đích vay", a.purpose), fact("Tài sản đảm bảo", a.collateral), fact("Thu nhập khai báo", a.income),
    ].join("");
    const docs = a.docs.length ? a.docs.map((d) => {
      const [t, c] = L.docStatus[d.status];
      const verify = state.role === "ADMIN" && !decided && d.status === "REVIEW" ? `<button class="btn-sm" data-sact="doc-ok" data-id="${a.id}" data-doc="${d.id}">Xác nhận</button>` : "";
      const reviewHint = (d.status === "MISSING" || d.status === "RESUBMIT") && d.hint && d.hint !== "Chưa có tệp" ? `<small class="doc-review-note">Lý do bổ sung: ${esc(d.hint)}</small>` : "";
      return `<div class="row"><button class="doc-open" type="button" data-sact="doc-view" data-id="${a.id}" data-doc="${esc(d.id)}" title="Xem chi tiết giấy tờ"><span class="icon-box">${icon(d.icon)}</span><span class="info"><p>${esc(d.name)}</p><small>${d.file ? esc(d.file) : esc(d.hint)}</small>${reviewHint}</span></button>${chip(t, c)}${verify}</div>`;
    }).join("") : `<p class="product-note">Database hiện chưa lưu tệp hồ sơ vay; tên, mục đích vay và thu nhập cũng chưa có cột tương ứng.</p>`;
    const header = `<div class="modal-head"><span class="icon-box">${icon("card")}</span><div><h3>Thông tin khoản vay</h3><small>${a.id}</small></div>${chip(st, sc)}<button class="btn-sm" data-sact="loan-close">Đóng</button></div>`;
    const dv = s.docView && a.docs.find((d) => d.id === s.docView);
    if (dv) return `<div class="modal-backdrop"><div class="modal-card wide">${header}${docDetail(a, dv, decided)}</div></div>`;
    let foot;
    if (s.mode === "reject") {
      foot = `<form id="reject-form" class="modal-form"><span class="eyebrow">Từ chối khoản vay</span>
        <div class="field"><label for="rj-note">${hasBackend() ? "Lý do từ chối (lưu vào nhật ký)" : "Lý do từ chối (gửi cho khách hàng)"}</label><div class="input-wrap area"><textarea id="rj-note" name="note" rows="3" required placeholder="Ví dụ: Thu nhập chưa đáp ứng điều kiện vay"></textarea></div></div>
        <div class="modal-actions"><button class="secondary" type="button" data-sact="loan-back">Quay lại</button><button class="primary" type="submit">Xác nhận từ chối</button></div></form>`;
    } else if (s.mode === "info") {
      const boxes = a.docs.map((d) => `<label class="check"><input type="checkbox" name="docs" value="${d.id}" ${d.status === "MISSING" || d.status === "RESUBMIT" || d.id === s.preDoc ? "checked" : ""} /> ${esc(d.name)}</label>`).join("");
      foot = `<form id="info-form" class="modal-form"><span class="eyebrow">Giấy tờ cần khách bổ sung</span>
        <div class="check-list">${boxes}</div>
        <div class="field"><label for="ix">Giấy tờ bổ sung khác (nếu có)</label><div class="input-wrap"><input id="ix" name="extra" maxlength="80" placeholder="Ví dụ: Hợp đồng mua bán tài sản" /></div><small class="product-note">Giấy tờ này sẽ được thêm vào danh sách để khách hàng tải lên và Admin xác nhận.</small></div>
        <div class="field"><label for="im">Nội dung gửi khách hàng</label><div class="input-wrap area"><textarea id="im" name="note" rows="3" required placeholder="Nêu rõ thông tin / giấy tờ cần bổ sung"></textarea></div></div>
        <div class="modal-actions"><button class="secondary" type="button" data-sact="loan-back">Quay lại</button><button class="primary" type="submit">Gửi yêu cầu bổ sung</button></div></form>`;
    } else if (decided) {
      foot = `${a.result && a.result.type !== "SUBMITTED" ? `<div class="notice ${a.stage < 0 ? "warn" : "ok"}"><b>${a.stage < 0 ? "Đã từ chối" : "Đã phê duyệt"}</b><p>${esc(a.result.note)}</p><small>${esc(a.result.by || "")} · ${a.result.time}</small></div>` : ""}${state.role === "ADMIN" && a.stage === 3 && a.status !== "DISBURSED" ? `<button class="primary" data-sact="loan-disburse" data-id="${a.id}">Giải ngân vào tài khoản thanh toán</button>` : ""}`;
    } else {
      const docsUnavailable = hasBackend() && a.docs.length === 0;
      const hint = docsUnavailable
        ? `<p class="product-note">Chưa có bảng lưu và tải lên tài liệu vay; hệ thống tạm khóa phê duyệt hồ sơ này.</p>`
        : unverified ? `<p class="product-note">${missing ? `Hồ sơ còn thiếu ${missing} giấy tờ` : "Còn giấy tờ đang chờ kiểm tra"} — cần xác nhận đủ hồ sơ trước khi phê duyệt.</p>` : "";
      const pendingNote = a.needInfo && a.result ? `<div class="notice warn"><b>Đang chờ khách bổ sung</b><p>${esc(a.result.note)}</p><small>${esc(a.result.by || "")} · ${a.result.time}</small></div>` : "";
      foot = `${pendingNote}${hint}<div class="loan-actions">${state.role === "ADMIN" ? `<button class="primary" data-sact="loan-approve" data-id="${a.id}" ${docsUnavailable || unverified || a.needInfo ? "disabled" : ""}>Phê duyệt</button><button class="secondary" data-sact="loan-reject-start">Từ chối</button>` : ""}<button class="secondary" data-sact="loan-info-start">Yêu cầu bổ sung</button></div>`;
    }
    return `
      <div class="modal-backdrop"><div class="modal-card wide">
        ${header}
        <div class="loan-facts">${facts}</div>
        <h4 class="modal-sub">Hồ sơ đính kèm <span class="modal-hint">· bấm vào từng giấy tờ để xem chi tiết</span></h4><div>${docs}</div>
        ${foot}
      </div></div>`;
  }

  const audit = () => `${head("An ninh hệ thống", "Nhật ký hoạt động")}
    <section class="form-card"><h2 class="display">Tra cứu theo mã</h2><form id="audit-lookup-form">
      ${fld("audit-lookup-id", "log_id", "Mã tra cứu / mã thực thi", "CASH_… hoặc LOG_…", "text", s.auditLookupQuery)}
      <button class="primary" type="submit">Tra cứu ${icon("search")}</button>
    </form>${s.auditLookupError ? `<p class="product-note">${esc(s.auditLookupError)}</p>` : ""}
    ${s.auditLookup ? `<div class="row"><span class="icon-box">${icon("list")}</span><div class="info"><p>${esc(s.auditLookup.action)} · ${esc(s.auditLookup.target_id)}</p><small>Mã: ${esc(s.auditLookup.log_id)} · Nhân viên: ${esc(s.auditLookup.operator_id)} · ${esc(s.auditLookup.created_at)}</small><small>${esc(s.auditLookup.details || "")}</small></div></div>` : ""}</section>
    <section class="surface">${auditRows(50)}</section>`;

  function staffPage() {
    const rows = s.staffMembers.map((member) => `
      <div class="row">
        <span class="icon-box">${icon("users")}</span>
        <div class="info"><p>${esc(member.fullname)} · ${esc(member.role === "ADMIN" ? "Quản trị viên" : "Giao dịch viên")}</p>
          <small>${esc(member.username)} · ${esc(member.email)} · ${esc(member.phone)}</small></div>
        <span class="chip ${member.status === "ACTIVE" ? "ok" : "warn"}">${esc(member.status === "ACTIVE" ? "Đang hoạt động" : "Đã khóa")}</span>
      </div>`).join("");
    return `${head("Quản trị nhân sự", "Nhân viên")}
      <div class="stack">
        <section class="form-card"><h2 class="display">Tạo tài khoản nhân viên</h2>
          <form id="staff-create-form">
            <div class="form-grid">
              ${fld("staff-username", "username", "Tên đăng nhập", "vd: teller01")}
              ${fld("staff-name", "fullname", "Họ và tên", "Nguyễn Văn A")}
              ${fld("staff-email", "email", "Email", "name@example.vn", "email", "", 'maxlength="254" title="Nhập email đúng định dạng, ví dụ name@example.vn"')}
              ${fld("staff-phone", "phone", "Số điện thoại", "10 chữ số", "tel", "", 'inputmode="numeric" pattern="[0-9]{10}" maxlength="10" data-digits-only title="Số điện thoại gồm đúng 10 chữ số"')}
              ${fld("staff-cccd", "cccd", "Số CCCD", "12 chữ số", "text", "", 'inputmode="numeric" pattern="[0-9]{12}" maxlength="12" data-digits-only title="CCCD gồm đúng 12 chữ số"')}
              ${fld("staff-password", "password", "Mật khẩu khởi tạo", "12+ ký tự, gồm chữ hoa/thường và số", "password")}
              <div class="field"><label for="staff-role">Vai trò</label><div class="input-wrap"><select id="staff-role" name="role" required><option value="TELLER">Giao dịch viên</option><option value="ADMIN">Quản trị viên</option></select></div></div>
            </div>
            <p class="product-note">Mỗi tài khoản nhân viên cần email, số điện thoại và CCCD chưa được dùng trong hệ thống.</p>
            <button class="primary" type="submit">Tạo nhân viên ${icon("check")}</button>
          </form>
        </section>
        <section class="surface"><div class="surface-title"><h2 class="display">Danh sách nhân viên</h2>
          ${hasBackend() ? '<button class="secondary" type="button" data-sact="reload-staff">Tải lại</button>' : ""}</div>
          ${rows || '<p class="product-note">Chưa có nhân viên.</p>'}
        </section>
      </div>`;
  }

  function settings() {
    const configDisabled = hasBackend() && !s.configLoaded ? "disabled" : "";
    const terms = L.terms.map((t) => `
      <div class="term-row">
        <label class="check"><input type="checkbox" name="on-${t.months}" ${t.enabled ? "checked" : ""} ${configDisabled} /> Kỳ hạn ${t.months} tháng</label>
        <div class="field"><label for="tr-${t.months}">Lãi suất vay (%/năm)</label><div class="input-wrap"><input id="tr-${t.months}" name="rate-${t.months}" type="number" step="0.1" min="0.1" max="50" value="${t.rate}" required ${configDisabled} /></div></div>
      </div>`).join("");
    return `${head("Tham số hệ thống", "Cấu hình")}
      <div class="stack">
      ${configDisabled ? '<p class="product-note">Đang tải cấu hình từ database; biểu mẫu sẽ mở khi tải thành công.</p>' : ""}
      <section class="form-card"><h2 class="display">Kỳ hạn vay</h2>
        <form id="terms-form">
          <div class="term-list">${terms}</div>
          <p class="product-note">Khách hàng chỉ thấy các kỳ hạn đang bật khi đăng ký vay. Hồ sơ đã nộp giữ nguyên lãi suất tại thời điểm đăng ký.</p>
          <button class="primary" type="submit" ${configDisabled}>Lưu kỳ hạn vay ${icon("check")}</button>
        </form>
      </section>
      <section class="form-card"><h2 class="display">Phí & lãi suất</h2>
        <form id="settings-form">
          <div class="form-grid">
            ${fld("f-inter", "interbank", "Phí chuyển liên ngân hàng (₫)", "9900", "number", s.fees.interbank)}
            ${fld("f-atm", "atm", "Phí rút ATM ngoại mạng (₫)", "3300", "number", s.fees.atm)}
          </div>
          ${fld("counter-limit", "counterLimit", "Hạn mức mỗi giao dịch tại quầy (₫)", "500000000", "number", s.fees.counterLimit)}
          ${fld("f-rate", "rate", "Lãi suất tiết kiệm tối đa (%/năm)", "6.5", "number", s.fees.rate)}
          <h3 class="display" style="font-size:1.3rem;margin-top:1.5rem">Tín dụng quay vòng</h3>
          <div class="form-grid">
            ${fld("credit-monthly-rate", "credit-monthly-rate", "Lãi suất tín dụng (%/tháng)", "2", "number", s.creditConfig.monthlyRate)}
            ${fld("credit-due-day", "credit-due-day", "Ngày đến hạn trong tháng (1–28)", "15", "number", s.creditConfig.dueDay)}
          </div>
          ${fld("credit-minimum-percent", "credit-minimum-percent", "Tỷ lệ trả tối thiểu (% dư nợ)", "5", "number", s.creditConfig.minimumPercent)}
          <p class="product-note">Cấu hình áp dụng khi Admin duyệt hạn mức mới. Mức thanh toán tối thiểu hiển thị theo tỷ lệ trên dư nợ hiện tại.</p>
          <button class="primary" type="submit" ${configDisabled}>Lưu cấu hình ${icon("check")}</button>
        </form>
      </section>
      </div>`;
  }

  const pages = { "open-account": openAccountPage, dashboard, customers: () => peoplePage(false), users: () => peoplePage(true), staff: staffPage, counter, loans: loansPage, credit: creditPage, audit, settings };

  return {
    get page() { return s.page; },
    // Nhận dữ liệu đã chuẩn hoá từ data.js (nạp từ database)
    hydrate(key, value) {
      if (key === "people") s.people = value;
      else if (key === "audit") s.audit = value;
      else if (key === "config") s.fees = { ...s.fees, ...value.fees };
      else if (key === "stats") s.stats = value;
    },
    login(identifier) {
      const v = identifier.trim().toLowerCase();
      state.role = v.startsWith("admin") ? "ADMIN" : /^(teller|gdv)/.test(v) ? "TELLER" : "CUSTOMER";
      s.page = "dashboard";
      s.q = "";
      s.oa = null;
      s.openAccountTarget = null;
      if (hasBackend()) {
        s.people.splice(0, s.people.length);
        s.staffMembers.splice(0, s.staffMembers.length);
        L.apps.splice(0, L.apps.length);
        s.audit.splice(0, s.audit.length);
        s.fees = { interbank: null, atm: null, rate: null };
        s.configLoaded = false;
        L.terms.splice(0, L.terms.length);
        s.stats = { today_transactions: null, today_served_customers: null, total_users: null, total_customers: null, total_transactions: null, total_balance_vnd: null, pending_loans: null, locked_users: null };
        loadDashboardStats().catch((error) => toast("Không tải được thống kê: " + (error?.message || String(error))));
        loadCustomers("").catch((error) => toast("Không tải được khách hàng: " + (error?.message || String(error))));
        loadLoanApplications().catch((error) => toast("Không tải được hồ sơ vay: " + (error?.message || String(error))));
        if (state.role === "ADMIN") loadAuditLogs().catch((error) => toast("Không tải được nhật ký: " + (error?.message || String(error))));
        if (state.role === "ADMIN") loadStaffMembers().catch((error) => toast("Không tải được nhân viên: " + (error?.message || String(error))));
        loadSystemConfig().catch((error) => toast("Không tải được cấu hình: " + (error?.message || String(error))));
      }
      s.detail = null;
      s.mode = null;
      s.docView = null;
      s.counterReceipt = null;
      s.auditLookup = null;
      s.auditLookupQuery = "";
      s.auditLookupError = "";
    },
    render() {
      const admin = state.role === "ADMIN";
      const nav = NAV[state.role].map(([page, image, title]) => [page, image, page === "users" && hasBackend() ? "Khách hàng" : title]);
      if (!nav.some((n) => n[0] === s.page) && !(s.page === "open-account" && !admin && s.oa)) s.page = "dashboard";
      const items = nav.map(([p, ic, t]) => `<button class="nav-button ${(s.page === p || (s.page === "open-account" && p === "customers")) ? "active" : ""}" data-spage="${p}">${icon(ic)} ${t}</button>`).join("");
      const mobile = nav.map(([p, ic, t]) => `<button class="${(s.page === p || (s.page === "open-account" && p === "customers")) ? "active" : ""}" data-spage="${p}">${icon(ic)}${t}</button>`).join("");
      app.innerHTML = `
        <header class="app-header"><div class="header-inner">${brand()}
          <nav class="desktop-nav">${items}</nav>
          <div class="header-actions"><span class="role-chip ${admin ? "admin" : "teller"}">${admin ? "ADMIN" : "TELLER"}</span>
            <button class="avatar" data-sact="logout" title="Đăng xuất"><b>${admin ? "AD" : "GD"}</b><span>${label()}</span></button></div>
        </div></header>
        <main class="page-container">${pages[s.page]()}</main>
        <nav class="mobile-nav">${mobile}</nav>${loanModal()}`;
    },
    click(event) {
      const el = event.target.closest("[data-spage],[data-sact]");
      if (!el || state.role === "CUSTOMER") return false;
      if (el.dataset.spage) {
        s.page = el.dataset.spage;
        s.q = "";
        window.scrollTo({ top: 0, behavior: "smooth" });
        render();
        if (s.page === "staff" && hasBackend()) {
          loadStaffMembers().catch((error) => toast("Không tải được nhân viên: " + (error?.message || String(error))));
        }
        if (s.page === "settings" && hasBackend()) {
          loadSystemConfig().catch((error) => toast("Không tải được cấu hình: " + (error?.message || String(error))));
        }
        if (s.page === "credit" && hasBackend()) loadCreditApplications().catch((error) => toast("Không tải được hồ sơ tín dụng: " + (error?.message || String(error))));
        return true;
      }
      const id = el.dataset.id;
      const act = el.dataset.sact;
      if (act === "credit-doc-review") {
        const action = el.dataset.action;
        let note = "";
        if (action === "REQUEST_RESUBMISSION") {
          note = String(prompt("Nhập lý do cần nộp lại giấy tờ thu nhập:") || "").trim();
          if (!note) { toast("Cần ghi rõ lý do để khách biết cách bổ sung."); return true; }
        }
        callBackend("creditBridge", "reviewDocument", {
          session_token: state.sessionToken, document_id: el.dataset.documentId, action, note,
        }).then(async (result) => {
          await loadCreditApplications();
          if (state.role === "ADMIN") await loadAuditLogs();
          toast(result.message || "Đã cập nhật giấy tờ tín dụng.");
        }).catch((error) => toast("Không cập nhật được giấy tờ: " + (error?.message || String(error))));
        return true;
      }
      if (act === "reload-credit" && hasBackend()) loadCreditApplications().catch((error) => toast(error?.message || String(error)));
      const person = s.people.find((p) => p.id === id);
      if (act === "reload-staff" && hasBackend()) {
        loadStaffMembers().catch((error) => toast("Không tải được nhân viên: " + (error?.message || String(error))));
      } else if (act === "logout") {
        onLogout();
        render();
      } else if (act === "toggle-lock" && person) {
        callBackend("userBridge", "toggleUserLock", { session_token: state.sessionToken, target_user_id: person.id })
          .then(() => { person.locked = !person.locked; toast(person.locked ? "Đã khóa tài khoản" : "Đã mở khóa tài khoản"); return state.role === "ADMIN" ? loadAuditLogs() : render(); })
          .catch((error) => toast("Không đổi được trạng thái: " + (error?.message || String(error))));
      } else if (act === "open-acc" && person && person.role === "CUSTOMER") {
        s.openAccountTarget = person.id;
        s.oa = oaNew();
        s.page = "open-account";
        window.scrollTo({ top: 0, behavior: "smooth" });
        render();
      } else if (act === "open-acc-close") {
        s.openAccountTarget = null;
        s.oa = null;
        s.page = "customers";
        render();
      } else if (act === "oa-type" && s.oa && Object.hasOwn(OA_PRODUCTS, id)) {
        if (s.oa.type === "CREDIT" && id !== "CREDIT") s.oa.v.CREDIT.file = null;
        s.oa.type = id;
        render();
      } else if (act === "oa-doc" && s.oa) {
        const docs = s.oa.v.LOAN.docs;
        docs[el.dataset.doc] = !docs[el.dataset.doc];
        render();
      } else if (act === "loan-view") {
        s.detail = id;
        s.mode = null;
        s.docView = null;
        s.preDoc = null;
        render();
      } else if (act === "loan-close") {
        s.detail = null;
        s.mode = null;
        s.docView = null;
        s.preDoc = null;
        render();
      } else if (act === "loan-back") {
        s.mode = null;
        s.preDoc = null;
        render();
      } else if (act === "doc-view") {
        s.docView = el.dataset.doc;
        render();
        const loan = L.apps.find((item) => item.id === id);
        const doc = loan?.docs.find((item) => item.id === el.dataset.doc);
        if (doc?.documentId) callBackend("loanBridge", "getDocumentLink", {
          session_token: state.sessionToken, document_id: doc.documentId,
        }).then((result) => { doc.url = result.url; render(); })
          .catch((error) => toast("Không mở được tài liệu: " + (error?.message || String(error))));
      } else if (act === "doc-back") {
        s.docView = null;
        render();
      } else if (act === "doc-redo") {
        s.preDoc = el.dataset.doc;
        s.docView = null;
        s.mode = "info";
        render();
      } else if (act === "loan-reject-start" || act === "loan-info-start") {
        s.mode = act === "loan-reject-start" ? "reject" : "info";
        render();
      } else if (act === "doc-ok") {
        const loan = L.apps.find((l) => l.id === id);
        const doc = loan && loan.docs.find((d) => d.id === el.dataset.doc);
        if (doc) {
          callBackend("loanBridge", "reviewDocument", { session_token: state.sessionToken,
            document_id: doc.documentId, action: "APPROVE", note: "" })
            .then(() => loadLoanApplications()).then(() => { s.detail = id; render(); })
            .catch((error) => toast("Không xác nhận được tài liệu: " + (error?.message || String(error))));
          return true;
        }
      } else if (act === "loan-approve") {
        const loan = L.apps.find((l) => l.id === id);
        if (loan && loan.docs.length && loan.docs.every((d) => d.status === "DONE") && !loan.needInfo) {
          callBackend("loanBridge", "approveLoan", { session_token: state.sessionToken, loan_id: id, action: "APPROVE", reason: "" })
            .then(async (result) => {
              await loadLoanApplications();
              if (state.role === "ADMIN") await loadAuditLogs();
              s.detail = id;
              toast(result.message || "Đã cập nhật phê duyệt và gửi thông báo khách hàng.");
              render();
            })
            .catch((error) => toast("Phê duyệt thất bại: " + (error?.message || String(error))));
          return true;
        }
      } else if (act === "loan-disburse") {
        if (state.role !== "ADMIN") { toast("Chỉ Admin được giải ngân khoản vay."); return true; }
        callBackend("loanBridge", "disburseLoan", { session_token: state.sessionToken, loan_id: id, target_account_id: "" })
          .then(async (result) => {
            await loadLoanApplications();
            await loadAuditLogs();
            s.detail = id;
            toast(result.message || "Đã giải ngân vào tài khoản thanh toán đang hoạt động của khách hàng.");
            render();
          }).catch((error) => toast("Giải ngân thất bại: " + (error?.message || String(error))));
        return true;
      }
      return true;
    },
    async submit(event) {
      const f = event.target;
      if (state.role === "CUSTOMER") return false;
      if (f.id === "audit-lookup-form" || f.id === "counter-lookup-form") {
        s.auditLookupQuery = String(new FormData(f).get("log_id") || "").trim();
        s.auditLookupError = "";
        try {
          const result = await callBackend("auditBridge", "getAuditLogById", {
            session_token: state.sessionToken, log_id: s.auditLookupQuery,
          });
          s.auditLookup = result.item;
        } catch (error) {
          s.auditLookup = null;
          s.auditLookupError = error?.message || String(error);
        }
        render();
        return true;
      }
      if (f.matches("[data-credit-decision]")) {
        if (state.role !== "ADMIN") { toast("Chỉ Admin được duyệt hạn mức."); return true; }
        const d = new FormData(f);
        const action = event.submitter?.value || "APPROVE";
        try {
          await callBackend("creditBridge", "decide", { session_token: state.sessionToken, application_id: String(d.get("application_id")), action, approved_limit_vnd: Number(d.get("limit") || 0), note: "" });
          await loadCreditApplications();
          toast(action === "APPROVE" ? "Đã duyệt và tạo hạn mức tín dụng." : "Đã từ chối hồ sơ tín dụng.");
        } catch (error) { toast("Không xử lý được hồ sơ: " + (error?.message || String(error))); }
        return true;
      }
      if (f.id === "open-account-form") {
        await openAccountSubmit(f);
        return true;
      }
      if (f.id === "staff-create-form") {
        const d = new FormData(f);
        const staff = {
          session_token: state.sessionToken,
          username: String(d.get("username") || "").trim(),
          fullname: String(d.get("fullname") || "").trim(),
          email: String(d.get("email") || "").trim(),
          phone: String(d.get("phone") || "").trim(),
          cccd: String(d.get("cccd") || "").trim(),
          role: String(d.get("role") || "TELLER"),
          password: String(d.get("password") || ""),
        };
        try {
          const result = await callBackend("staffBridge", "createStaff", staff);
          await loadStaffMembers();
          await loadAuditLogs();
          f.reset();
          toast(`Đã tạo tài khoản nhân viên ${result.created_id}`);
        } catch (error) {
          toast("Không tạo được nhân viên: " + (error?.message || String(error)));
        }
        return true;
      }
      if (f.id === "staff-search") {
        s.q = f.elements.q.value.trim();
        try { await loadCustomers(s.q); } catch (error) { toast("Tra cứu thất bại: " + (error?.message || String(error))); }
        return true;
      }
      if (f.id === "counter-form") {
        const d = new FormData(f);
        const deposit = d.get("type") === "DEPOSIT";
        const amount = Number(d.get("amount"));
        const digits = String(d.get("acc")).replace(/\D/g, "");
        const target = s.people.find((p) => hasAcc(p) && p.acc.replace(/\D/g, "") === digits);
        const targetAccountId = target?.acc || String(d.get("acc") || "").trim();
        const fingerprint = JSON.stringify([deposit ? "DEPOSIT" : "WITHDRAW", targetAccountId, amount]);
        const pending = readCounterPending();
        if (pending && pending.fingerprint !== fingerprint) {
          toast("Có giao dịch trước chưa rõ kết quả. Hãy gửi lại đúng tài khoản, loại giao dịch và số tiền để tra cứu trước.");
          return true;
        }
        const executionId = pending?.executionId || counterExecutionId();
        if (!pending) writeCounterPending({ executionId, fingerprint });
        s.auditLookup = null;
        s.auditLookupQuery = "";
        s.auditLookupError = "";
        try {
          const result = deposit
            ? await callBackend("transactionBridge", "depositCounter", { session_token: state.sessionToken, target_account_id: targetAccountId, amount_vnd: amount, description: "Nạp tiền tại quầy", execution_id: executionId })
            : await callBackend("transactionBridge", "withdrawCounter", { session_token: state.sessionToken, target_account_id: targetAccountId, amount_vnd: amount, pin: String(d.get("pin") || ""), execution_id: executionId });
          if (target) setBal(target, Number(result.post_balance_vnd || 0));
          s.counterReceipt = {
            executionId: result.execution_id || "",
            transactionId: result.transaction_id || "",
            success: true,
            tracePersisted: result.trace_persisted === true,
            message: result.message || `${deposit ? "Nạp" : "Rút"} tiền thành công${target ? ` · số dư mới ${money(target.bal)} ₫` : ""}`,
          };
          writeCounterPending(null);
          toast(`${deposit ? "Nạp" : "Rút"} thành công · mã tra cứu ${s.counterReceipt.executionId}`);
          f.reset();
          render();
          if (state.role === "ADMIN") loadAuditLogs().catch(() => {});
        } catch (error) {
          // Backend replies (including a failed, traced result) are definitive.
          // A timeout has no execution_id, so retain the same ID for the retry.
          if (error?.executionId) writeCounterPending(null);
          s.counterReceipt = {
            executionId: error?.executionId || executionId,
            transactionId: "",
            success: false,
            tracePersisted: error?.tracePersisted === true,
            message: error?.message || String(error),
          };
          render();
          toast(`Giao dịch tại quầy thất bại · mã tra cứu ${s.counterReceipt.executionId}`);
          if (state.role === "ADMIN" && s.counterReceipt.tracePersisted) loadAuditLogs().catch(() => {});
        }
        return true;
      }
      if (f.id === "reject-form" || f.id === "info-form") {
        const loan = L.apps.find((l) => l.id === s.detail);
        if (!loan) return true;
        const d = new FormData(f);
        const note = String(d.get("note")).trim();
        if (f.id === "reject-form") {
          try {
            const result = await callBackend("loanBridge", "approveLoan", { session_token: state.sessionToken, loan_id: loan.id, action: "REJECT", reason: note });
            await loadLoanApplications();
            if (state.role === "ADMIN") await loadAuditLogs();
            s.detail = loan.id;
            s.mode = null;
            toast(result.message || "Đã cập nhật từ chối, lưu lý do và gửi thông báo khách hàng.");
            render();
          } catch (error) { toast("Từ chối thất bại: " + (error?.message || String(error))); }
          return true;
        } else {
          const picked = d.getAll("docs");
          const extra = String(d.get("extra") || "").trim();
          if (!picked.length && !extra) {
            toast("Chọn ít nhất một giấy tờ cần bổ sung.");
            return true;
          }
          try {
            const sent = [];
            for (const docType of picked) {
              const doc = loan.docs.find((item) => item.id === docType);
              if (!doc?.documentId) throw new Error("Không tìm thấy giấy tờ trong hồ sơ.");
              const result = await callBackend("loanBridge", "reviewDocument", { session_token: state.sessionToken,
                document_id: doc.documentId, action: "REQUEST_RESUBMISSION", note });
              sent.push(result.message);
            }
            if (extra) {
              const result = await callBackend("loanBridge", "reviewDocument", {
                session_token: state.sessionToken, loan_id: loan.id, action: "REQUEST_ADDITIONAL_DOCUMENT",
                document_name: extra, note,
              });
              sent.push(result.message);
            }
            await loadLoanApplications();
            if (state.role === "ADMIN") await loadAuditLogs();
            s.detail = loan.id;
            s.mode = null;
            toast(sent.some((message) => String(message || "").includes("chưa gửi được thông báo"))
              ? "Đã lưu yêu cầu bổ sung nhưng chưa gửi được thông báo."
              : "Đã lưu yêu cầu bổ sung và thông báo cho khách hàng.");
            render();
          } catch (error) { toast("Không lưu được yêu cầu bổ sung: " + (error?.message || String(error))); }
          return true;
        }
        s.mode = null;
        s.preDoc = null;
        render();
        return true;
      }
      if (f.id === "terms-form") {
        const d = new FormData(f);
        const next = L.terms.map((t) => ({ months: t.months, enabled: d.get(`on-${t.months}`) !== null, rate: Number(d.get(`rate-${t.months}`)) }));
        if (!next.some((t) => t.enabled)) { toast("Cần bật ít nhất một kỳ hạn vay"); return true; }
        if (next.some((t) => !(t.rate > 0 && t.rate <= 50))) { toast("Lãi suất phải trong khoảng 0 – 50%/năm"); return true; }
        try {
          await saveSystemConfig(s.fees, next);
          toast("Đã lưu kỳ hạn vay vào database");
          render();
        } catch (error) { toast("Không lưu được kỳ hạn vay: " + (error?.message || String(error))); }
        return true;
      }
      if (f.id === "settings-form") {
        const d = new FormData(f);
        const fees = { interbank: +d.get("interbank"), atm: +d.get("atm"), rate: +d.get("rate"), counterLimit: +d.get("counterLimit") };
        const nextCredit = { monthlyRate: +d.get("credit-monthly-rate"), dueDay: +d.get("credit-due-day"), minimumPercent: +d.get("credit-minimum-percent") };
        if (!Number.isSafeInteger(fees.counterLimit) || fees.counterLimit < 1000000 || fees.counterLimit > 1000000000) { toast("Hạn mức mỗi giao dịch tại quầy phải từ 1.000.000 đến 1.000.000.000 đồng"); return true; }
        if (!(nextCredit.monthlyRate >= 0 && nextCredit.monthlyRate <= 20) || !(nextCredit.dueDay >= 1 && nextCredit.dueDay <= 28) || !(nextCredit.minimumPercent >= 1 && nextCredit.minimumPercent <= 100)) { toast("Cấu hình tín dụng không hợp lệ"); return true; }
        s.creditConfig = nextCredit;
        try {
          await saveSystemConfig(fees, L.terms);
          toast("Đã lưu cấu hình vào database");
          render();
        } catch (error) { toast("Không lưu được cấu hình: " + (error?.message || String(error))); }
        return true;
      }
      return false;
    },
  };
}
