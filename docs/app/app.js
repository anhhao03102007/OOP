import { createStaff } from "./staff.js";
import { createData } from "./data.js";

// Wait until Qt publishes its C++ bridges; in a regular browser this resolves immediately.
await globalThis.__OOPBANK_BRIDGES_READY__;

const app = document.querySelector("#app");
app.addEventListener("input", (event) => {
  const input = event.target;
  if (!(input instanceof HTMLInputElement)) return;
  input.classList.add("touched");
  if (input.hasAttribute("data-digits-only")) {
    const start = input.selectionStart;
    const digits = input.value.replace(/[^0-9]/g, "");
    if (digits !== input.value) {
      input.value = digits;
      input.setSelectionRange(Math.min(start ?? digits.length, digits.length), Math.min(start ?? digits.length, digits.length));
    }
  }
  input.closest(".input-wrap")?.classList.toggle("invalid", !input.validity.valid);
});
const CLOUDINARY_CLOUD_NAME = "ltcqqgmq";
const CLOUDINARY_UPLOAD_PRESET = "oopbank_unsigned";
const numericIdempotencyKey = (prefix) =>
  `${prefix}_${Date.now()}${Array.from({ length: 20 }, () => Math.floor(Math.random() * 10)).join("")}`;

function getBridge(name) {
  return globalThis[name]
    ?? globalThis.oopbank?.[name]
    ?? globalThis.oopbankBridge?.[name]
    ?? null;
}

function hasBackend() {
  return Boolean(getBridge("userBridge"));
}

function getAuthBridge() {
  return getBridge("authBridge");
}

function parseBridgeResponse(value) {
  return typeof value === "string" ? JSON.parse(value) : value;
}

function invokeBridgeMethod(bridge, methodName, args) {
  return new Promise((resolve, reject) => {
    let settled = false;
    const finish = (callback, value) => {
      if (settled) return;
      settled = true;
      clearTimeout(timer);
      callback(value);
    };
    const timer = setTimeout(() => finish(reject, new Error("Hết thời gian chờ xác thực.")), 15000);
    const callback = (value) => finish(resolve, value);

    try {
      const result = bridge[methodName](...args, callback);
      if (result && typeof result.then === "function") {
        result.then(callback, (error) => finish(reject, error));
      } else if (result !== undefined) {
        callback(result);
      }
    } catch (error) {
      finish(reject, error);
    }
  });
}

async function callBackend(bridgeName, methodName, payload) {
  const bridge = getBridge(bridgeName);
  if (!bridge || typeof bridge[methodName] !== "function") {
    throw new Error(`Backend ${bridgeName}.${methodName} chưa sẵn sàng.`);
  }
  const response = parseBridgeResponse(await invokeBridgeMethod(
    bridge,
    methodName,
    [JSON.stringify(payload)],
  ));
  if (!response || response.success !== true) {
    const error = new Error(response?.message || "Backend không xác nhận thao tác thành công.");
    if (response?.execution_id) error.executionId = String(response.execution_id);
    error.tracePersisted = response?.trace_persisted === true;
    throw error;
  }
  return response;
}

const state = {
  loggedIn: false,
  userId: "",
  sessionToken: globalThis.__OOPBANK_SESSION_TOKEN__ ?? "",
  role: "CUSTOMER", // CUSTOMER | TELLER | ADMIN
  authMode: "login",
  page: "home",
  showBalance: true,
  billType: "electricity",
  billProviders: [],
  billProvidersLoaded: false,
  billProvidersError: "",
  selectedMessage: 0,
  transactionSuccess: false,
  transactionSuccessMessage: "",
  showPinModal: false, // State quản lý Pop-up PIN/OTP
  pendingTransaction: null, // Lưu dữ liệu form chờ xác thực PIN
  loanTab: "apply", // apply | docs | review | disburse | manage
  openSchedule: null, // id khoản vay đang mở lịch trả nợ
  docAppId: null, // đơn vay đang bổ sung hồ sơ
  balance: 0,
  profile: { name: "", email: "", phone: "", branch: "OOPBank" },
  accounts: [],
  savings: [],
  creditAccount: null,
  creditLoading: false,
  creditError: "",
  creditApplications: [],
  msgOpen: false, // hộp thư nổi đang mở
  msgExpanded: null, // tin nhắn đang xem chi tiết trong hộp thư nổi
  msgBanner: null, // tin nhắn mới đang hiện dạng banner
};

const icons = {
  arrow: '<path d="M5 12h14"/><path d="m13 6 6 6-6 6"/>',
  back: '<path d="M19 12H5"/><path d="m11 18-6-6 6-6"/>',
  bell: '<path d="M18 8a6 6 0 0 0-12 0c0 7-3 7-3 9h18c0-2-3-2-3-9"/><path d="M10 21h4"/>',
  card: '<rect x="2" y="5" width="20" height="14" rx="3"/><path d="M2 10h20M6 15h4"/>',
  check: '<path d="m5 12 4 4L19 6"/>',
  eye: '<path d="M2 12c2.5-4.5 6-7 10-7s7.5 2.5 10 7c-2.5 4.5-6 7-10 7s-7.5-2.5-10-7Z"/><circle cx="12" cy="12" r="2.5"/>',
  eyeOff: '<path d="m3 3 18 18"/><path d="M10.6 6.2A10 10 0 0 1 12 6c6 0 9.5 6 9.5 6"/><path d="M6.6 6.6C4 8.2 2.5 12 2.5 12s3.5 6 9.5 6"/>',
  grid: '<rect x="3" y="3" width="7" height="7" rx="2"/><rect x="14" y="3" width="7" height="7" rx="2"/><rect x="3" y="14" width="7" height="7" rx="2"/><rect x="14" y="14" width="7" height="7" rx="2"/>',
  home: '<path d="m3 11 9-8 9 8"/><path d="M5 10v11h14V10M9 21v-6h6v6"/>',
  lock: '<rect x="4" y="10" width="16" height="11" rx="2"/><path d="M8 10V7a4 4 0 0 1 8 0v3"/>',
  mail: '<rect x="3" y="5" width="18" height="14" rx="2"/><path d="m3 7 9 6 9-6"/>',
  phone: '<path d="M7 2h10a2 2 0 0 1 2 2v16a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2Z"/><path d="M10 18h4"/>',
  plus: '<path d="M12 5v14M5 12h14"/>',
  send: '<path d="m22 2-7 20-4-9-9-4Z"/><path d="M22 2 11 13"/>',
  shield: '<path d="M12 22s8-3.5 8-10V5l-8-3-8 3v7c0 6.5 8 10 8 10Z"/><path d="m9 12 2 2 4-5"/>',
  user: '<circle cx="12" cy="8" r="4"/><path d="M4 22a8 8 0 0 1 16 0"/>',
  wallet: '<path d="M4 6h14a2 2 0 0 1 2 2v12H4a2 2 0 0 1-2-2V5a3 3 0 0 1 3-3h13"/><path d="M15 12h7v5h-7a2.5 2.5 0 0 1 0-5Z"/>',
  key: '<path d="m21 2-2 2m-1.5 1.5L14 9l-1.5-1.5L11 9l-1.5-1.5L8 9 2 15v5h5l1.5-1.5L7 17l1.5-1.5L7 14l5.5-5.5z"/>',
  clock: '<circle cx="12" cy="12" r="9"/><path d="M12 7v5l3 2"/>',
  file: '<path d="M14 3H7a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V8z"/><path d="M14 3v5h5M9 13h6M9 17h4"/>',
  settings: '<circle cx="12" cy="12" r="3"/><path d="M12 2v3M12 19v3M2 12h3M19 12h3M5 5l2 2M17 17l2 2M19 5l-2 2M7 17l-2 2"/>',
};

function icon(name) {
  return `<svg class="icon" aria-hidden="true" viewBox="0 0 24 24">${icons[name] || icons.grid}</svg>`;
}

function setPasswordVisibility(button, visible) {
  const passwordInput = document.getElementById(button?.dataset.passwordTarget || "");
  if (!passwordInput) return;
  passwordInput.type = visible ? "text" : "password";
  button.innerHTML = icon(visible ? "eye" : "eyeOff");
  button.setAttribute("aria-label", visible ? "Ẩn mật khẩu" : "Hiện mật khẩu");
  button.setAttribute("aria-pressed", String(visible));
}

function brand() {
  return `
    <div class="brand">
      <span class="brand-mark">${icon("home")}</span>
      <span><span class="brand-name">OOPBANK</span><span class="brand-sub">Private Banking</span></span>
    </div>`;
}

function field(id, label, iconName, placeholder, type = "text", val = "", attributes = "") {
  const passwordAction = type === "password"
    ? `<button class="password-toggle" type="button" data-action="toggle-password" data-password-target="${id}" aria-label="Hiện mật khẩu" aria-pressed="false">${icon("eyeOff")}</button>`
    : "";
  return `
    <div class="field">
      <label for="${id}">${label}</label>
      <div class="input-wrap">${icon(iconName)}<input id="${id}" name="${id}" type="${type}" value="${val}" placeholder="${placeholder}" required ${attributes} />${passwordAction}</div>
    </div>`;
}

function renderAuth() {
  if (state.authMode === "forgot") return renderForgotPassword();
  const register = state.authMode === "register";
  app.innerHTML = `
    <main class="auth-shell">
      <section class="auth-card">
        <aside class="auth-visual">
          <div class="visual-top">${brand()}</div>
          <div class="visual-copy">
            <h2 class="display">Vững tâm hôm nay,<br />thịnh vượng ngày mai.</h2>
            <p>Đồng hành cùng bạn kiến tạo một tương lai tài chính bền vững với dịch vụ ngân hàng riêng tư và tận tâm.</p>
          </div>
        </aside>
        <section class="auth-panel">
          <button class="auth-help" type="button">Trợ giúp</button>
          <div class="auth-form-wrap">
            <span class="eyebrow">${register ? "Bắt đầu hành trình" : "Chào mừng trở lại"}</span>
            <h1 class="display">${register ? "Tạo tài khoản mới" : "Đăng nhập tài khoản"}</h1>
            <p class="subcopy">${register ? "Chỉ mất vài phút để gia nhập OOPBank." : "Quản lý tài chính của bạn an toàn, mọi lúc mọi nơi."}</p>
            <div class="tabs">
              <button class="tab ${register ? "" : "active"}" data-auth-mode="login">Đăng nhập</button>
              <button class="tab ${register ? "active" : ""}" data-auth-mode="register">Đăng ký</button>
            </div>
            <form id="auth-form" class="form-stack">
              ${register ? field("username", "Tên đăng nhập", "user", "Tên đăng nhập") : ""}
              ${register ? field("name", "Họ và tên", "user", "Nguyễn Minh Anh") : ""}
              ${field("contact", register ? "Địa chỉ email" : "Tên đăng nhập hoặc số điện thoại", "mail", register ? "tenban@email.com" : "Tên đăng nhập", register ? "email" : "text")}
              ${register ? field("phone", "Số điện thoại", "phone", "10 hoặc 11 chữ số", "tel", "", 'inputmode="numeric" pattern="[0-9]{10,11}" maxlength="11" data-digits-only title="Số điện thoại chỉ gồm 10 hoặc 11 chữ số"') : ""}
              ${register ? field("cccd", "Số CCCD", "shield", "Nhập 12 số căn cước công dân", "text", "", 'inputmode="numeric" pattern="[0-9]{12}" maxlength="12" data-digits-only title="CCCD gồm đúng 12 chữ số"') : ""}
              ${field("password", "Mật khẩu", "lock", register ? "Tối thiểu 8 ký tự" : "Nhập mật khẩu của bạn", "password")}
              ${register ? "" : '<div class="forgot-row"><button class="link-button" type="button" data-action="forgot-pass">Quên mật khẩu?</button></div>'}
              <button class="primary" type="submit">${register ? "Tạo tài khoản" : "Đăng nhập an toàn"} ${icon("arrow")}</button>
            </form>
          </div>
          <footer class="auth-footer">© 2026 OOPBank</footer>
        </section>
      </section>
    </main>`;
}

function renderForgotPassword() {
  app.innerHTML = `
    <main class="auth-shell">
      <section class="auth-card">
        <aside class="auth-visual">
          <div class="visual-top">${brand()}</div>
          <div class="visual-copy">
            <h2 class="display">An toàn tài khoản,<br />vững bước tương lai.</h2>
            <p>Xác minh thông tin đã đăng ký để đặt lại mật khẩu OOPBank.</p>
          </div>
        </aside>
        <section class="auth-panel">
          <button class="auth-help" type="button">Trợ giúp</button>
          <div class="auth-form-wrap forgot-form-wrap">
            <button class="auth-back-button" type="button" data-auth-mode="login">${icon("back")} Quay lại đăng nhập</button>
            <span class="eyebrow">Khôi phục quyền truy cập</span>
            <h1 class="display">Đặt lại mật khẩu</h1>
            <p class="subcopy">Nhập thông tin tài khoản, CCCD và mã PIN giao dịch để xác minh danh tính.</p>
            <form id="forgot-password-form" class="form-stack">
              ${field("forgot-identifier", "Tên đăng nhập, email hoặc số điện thoại", "user", "Thông tin đăng nhập đã đăng ký")}
              ${field("forgot-cccd", "Số CCCD đã đăng ký", "shield", "Nhập 12 số CCCD", "text", "", 'inputmode="numeric" pattern="[0-9]{12}" maxlength="12" data-digits-only title="CCCD gồm đúng 12 chữ số"')}
              ${field("forgot-pin", "Mã PIN giao dịch", "key", "Nhập mã PIN 6 số", "password")}
              ${field("forgot-new-password", "Mật khẩu mới", "lock", "Tối thiểu 8 ký tự", "password")}
              ${field("forgot-confirm-password", "Xác nhận mật khẩu mới", "lock", "Nhập lại mật khẩu mới", "password")}
              <p class="auth-security-note">Mật khẩu mới cần có ít nhất 8 ký tự. Không chia sẻ mã PIN với người khác.</p>
              <button class="primary" type="submit">Xác minh và đặt lại mật khẩu ${icon("arrow")}</button>
            </form>
          </div>
          <footer class="auth-footer">© 2026 OOPBank</footer>
        </section>
      </section>
    </main>`;
}

const transactions = [];

function transactionRows(all = false) {
  const rows = all ? transactions : transactions.slice(0, 3);
  if (!rows.length) return '<p class="product-note">Chưa có giao dịch nào.</p>';
  return rows.map(([name, detail, amount, iconName]) => `
    <div class="transaction">
      <span class="icon-box">${icon(iconName)}</span>
      <div class="info"><p>${escHtml(name)}</p><small>${escHtml(detail)}</small></div>
      <span class="amount ${amount.startsWith("+") ? "positive" : ""}">${escHtml(amount)}</span>
    </div>`).join("");
}

// Thanh điều hướng khách hàng — cùng cấu trúc [trang, biểu tượng, nhãn] với Teller/Admin (staff.js)
const NAV_CUSTOMER = [["home", "home", "Trang chủ"], ["accounts", "wallet", "Tài khoản"], ["services", "grid", "Dịch vụ"], ["user-settings", "settings", "Cài đặt"]];
// Trang con của dịch vụ → mục điều hướng cha được tô sáng
const NAV_PARENT = { transactions: "accounts", transfer: "services", topup: "services", payment: "services", savings: "services", credit: "services", "apply-loan": "services", "loan-pay": "services" };
const navActive = () => NAV_PARENT[state.page] || state.page;
const initials = (name) => { const w = String(name).trim().split(/\s+/); return ((w[0]?.[0] || "") + (w.length > 1 ? w[w.length - 1][0] : "")).toUpperCase(); };
const shortName = (name) => String(name).trim().split(/\s+/).slice(-2).join(" ");

function appHeader() {
  const items = NAV_CUSTOMER.map(([p, ic, t]) => `<button class="nav-button ${navActive() === p ? "active" : ""}" data-page="${p}">${icon(ic)} ${t}</button>`).join("");
  return `
    <header class="app-header">
      <div class="header-inner">
        ${brand()}
        <nav class="desktop-nav">${items}</nav>
        <div class="header-actions">
          <span class="role-chip customer">KHÁCH HÀNG</span>
          <button class="circle-button" data-page="notifications" aria-label="Thông báo">${icon("bell")}${messages.some((m) => m.unread) ? '<i class="unread"></i>' : ""}</button>
          <button class="avatar" data-action="logout" title="Đăng xuất"><b>${escHtml(initials(state.profile.name))}</b><span>${escHtml(shortName(state.profile.name))}</span></button>
        </div>
      </div>
    </header>`;
}

function mobileNav() {
  return `
    <nav class="mobile-nav">
      <button class="${state.page === "home" ? "active" : ""}" data-page="home">${icon("home")}Trang chủ</button>
      <button class="${state.page === "accounts" ? "active" : ""}" data-page="accounts">${icon("wallet")}Tài khoản</button>
      <button class="${state.page === "transfer" ? "active" : ""}" data-page="transfer">${icon("send")}Chuyển tiền</button>
      <button class="${state.page === "services" ? "active" : ""}" data-page="services">${icon("grid")}Tiện ích</button>
    </nav>`;
}

function renderHome() {
  app.innerHTML = `
    ${appHeader()}
    <main class="page-container">
      <div class="welcome">
        <div><p>${greeting()},</p><h1 class="display">${escHtml(state.profile.name)}</h1></div>
        <time>${todayText()}</time>
      </div>
      <div class="grid-main">
        <section class="balance-card">
          <span class="balance-label">Số dư tài khoản chính</span>
          <div class="balance-line">
            <span class="balance">${state.showBalance ? vnd(state.balance) : "••••••••••"}</span>
            <button class="ghost-button" data-action="toggle-balance">${icon(state.showBalance ? "eye" : "eyeOff")}</button>
          </div>
          <div class="account-meta">
            <div><small>${escHtml(primaryAcc().name)}</small><b>${escHtml(primaryAcc().number)}</b></div>
            <div><small>Chi nhánh</small><b>${escHtml(state.profile.branch)}</b></div>
          </div>
        </section>
        <section class="surface">
          <div class="surface-title"><h2 class="display">Thao tác nhanh</h2><button class="text-action" data-page="services">Chỉnh sửa</button></div>
          <div class="quick-grid">
            ${quickButton("transfer", "send", "Chuyển tiền")}
            ${quickButton("topup", "plus", "Nạp tiền")}
            ${quickButton("payment", "card", "Thanh toán")}
            ${quickButton("transactions", "clock", "Giao dịch")}
            ${quickButton("savings", "wallet", "Tiết kiệm")}
            ${quickButton("loan", "card", "Khoản vay")}
          </div>
        </section>
      </div>
      <div class="content-grid single">
        <section class="surface">
          <div class="surface-title"><h2 class="display">Giao dịch gần đây</h2><button class="text-action" data-page="transactions">Xem tất cả</button></div>
          ${transactionRows()}
        </section>
        <section class="surface promo">
          <span class="eyebrow">Tiết kiệm thông minh</span>
          <h2 class="display">Sinh lời mỗi ngày, vững vàng tương lai.</h2>
          <p>Lãi suất hấp dẫn lên đến 6,1%/năm với Tiết kiệm OOPBank.</p>
          <button class="primary" data-page="savings">Khám phá ngay ${icon("arrow")}</button>
        </section>
      </div>
    </main>
    ${mobileNav()}`;
}

function quickButton(page, iconName, label) {
  return `<button class="quick" data-page="${page}"><span>${icon(iconName)}</span>${label}</button>`;
}

function productCard(page, iconName, eyebrow, title, value, note, button, progress = false) {
  return `
    <section class="surface">
      <div class="product-head">
        <div class="product-title"><span class="icon-box">${icon(iconName)}</span><div><span class="eyebrow">${eyebrow}</span><h3>${title}</h3></div></div>
        <span class="status">${progress ? "Còn 14 kỳ" : "Đang hoạt động"}</span>
      </div>
      <p class="product-value">${value}</p><p class="product-note">${note}</p>
      ${progress ? '<div class="progress"><i></i></div>' : ""}
      <button class="secondary" data-page="${page}">${button}</button>
    </section>`;
}

const pageDetails = {
  transfer: ["Giao dịch 24/7", "Chuyển tiền", "Chuyển tiền nhanh chóng và an toàn đến mọi ngân hàng.", "send"],
  topup: ["Nạp tiền tức thì", "Nạp tiền điện thoại", "Nạp tiền cho bạn và người thân với nhiều ưu đãi.", "phone"],
  payment: ["Thanh toán hóa đơn", "Thanh toán", "Thanh toán hóa đơn Điện, Nước và Internet tại một nơi.", "card"],
  services: ["OOPBank tiện ích", "Tất cả dịch vụ", "Khám phá đầy đủ các dịch vụ tài chính dành cho bạn.", "grid"],
  accounts: ["Tài sản của tôi", "Tài khoản", "Theo dõi số dư và quản lý các tài khoản OOPBank.", "wallet"],
  notifications: ["Cập nhật mới nhất", "Thông báo", "Tin nhắn tiền vào, chuyển tiền và xác thực OTP.", "bell"],
  transactions: ["Lịch sử tài chính", "Tất cả giao dịch", "Theo dõi chi tiết biến động số dư trên tài khoản.", "grid"],
  savings: ["Tích lũy sinh lời", "Sổ tiết kiệm", "Mở sổ trực tuyến và tất toán chủ động.", "wallet"],
  loan: ["Quản lý tín dụng", "Khoản vay", "Theo dõi dư nợ và thanh toán các kỳ vay đúng hạn.", "card"],
  "apply-loan": ["Tín dụng linh hoạt", "Vay vốn", "Đăng ký, bổ sung hồ sơ, theo dõi xử lý, giải ngân và quản lý khoản vay tại một nơi.", "card"],
  "loan-pay": ["Quản lý tín dụng", "Thanh toán kỳ vay", "Thanh toán kỳ vay đến hạn từ tài khoản thanh toán.", "card"],
  "user-settings": ["An toàn & Bảo mật", "Cài đặt tài khoản", "Quản lý thông tin cá nhân, mật khẩu và mã PIN giao dịch.", "settings"],
};

function featureShell(content) {
  const detail = pageDetails[state.page] || ["OOPBank", "Dịch vụ", "Hệ thống ngân hàng số.", "grid"];
  return `
    ${appHeader()}
    <main class="feature-container">
      <button class="back" data-page="home">${icon("back")} Quay lại</button>
      <div class="feature-title"><span class="icon-box">${icon(detail[3])}</span><div><span class="eyebrow">${detail[0]}</span><h1 class="display">${detail[1]}</h1></div></div>
      <p class="feature-description">${detail[2]}</p>
      ${content}
    </main>
    ${mobileNav()}
    ${renderPinModal()}`;
}

// 1. MODAL XÁC THỰC MÃ PIN & OTP (BẢO MẬT GIAO DỊCH)
function renderPinModal() {
  if (!state.showPinModal) return "";
  const needsOtp = state.pendingTransaction && state.pendingTransaction.type === "transfer" && state.pendingTransaction.amount > 50000000;
  const otpWaiting = needsOtp && hasBackend() && !state.pendingTransaction.otpChallengeId;
  const pending = state.pendingTransaction;
  const simulationNotice = pending?.type === "payment"
    ? '<p class="product-note" role="status">Thanh toán mô phỏng trong OOPBank; chưa xác nhận với nhà cung cấp.</p>'
    : pending?.type === "topup"
      ? '<p class="product-note" role="status">Nạp điện thoại mô phỏng; chưa gửi yêu cầu tới nhà mạng.</p>'
      : pending?.type === "transfer" && pending.data?.bank && pending.data.bank !== "OOPBANK"
        ? '<p class="product-note" role="status">Chuyển liên ngân hàng mô phỏng; chưa gửi yêu cầu tới ngân hàng nhận.</p>' : "";
  return `
    <div class="modal-backdrop">
      <div class="modal-card">
        <div class="modal-head">
          <span class="icon-box">${icon("shield")}</span>
          <div>
            <h3>Xác thực giao dịch</h3>
            <small>Vui lòng nhập mã PIN giao dịch để hoàn tất</small>
          </div>
        </div>
        ${pendingSummary()}
        ${simulationNotice}
        <form id="pin-modal-form" class="form-stack">
          <div class="field">
            <label for="modal-pin">Mã PIN giao dịch (6 số)</label>
            <div class="input-wrap">
              ${icon("key")}
              <input id="modal-pin" name="pin" type="password" maxlength="6" pattern="[0-9]{6}" placeholder="••••••" required autofocus />
            </div>
          </div>
          ${needsOtp ? `
            <div class="field">
              ${otpWaiting ? `<label for="modal-otp">Đang tạo mã xác nhận trong ứng dụng…</label>` : `<label for="modal-otp">Mã xác nhận giao dịch (&gt; 50 triệu VNĐ)</label>
              ${state.pendingTransaction.otpCode ? `<div class="otp-preview" role="status"><span>Mã do ứng dụng tạo · hiệu lực 3 phút</span><strong>${escHtml(state.pendingTransaction.otpCode)}</strong></div>` : ""}`}
              <div class="input-wrap">
                ${icon("shield")}
                <input id="modal-otp" name="otp" type="text" inputmode="numeric" autocomplete="one-time-code" maxlength="6" pattern="[0-9]{6}" placeholder="Nhập lại mã 6 số ở trên" ${otpWaiting ? "disabled" : "required"} />
              </div>
            </div>` : ""}
          <div class="modal-actions">
            <button class="secondary" type="button" data-action="close-modal">Hủy bỏ</button>
            <button class="primary" type="submit" ${otpWaiting ? "disabled" : ""}>Xác nhận giao dịch ${icon("check")}</button>
          </div>
        </form>
      </div>
    </div>`;
}

function renderServices() {
  const services = [
    ["transfer", "send", "Chuyển tiền"], ["topup", "phone", "Nạp điện thoại"],
    ["payment", "card", "Thanh toán"], ["savings", "wallet", "Tiết kiệm"],
    ["credit", "card", "Tín dụng quay vòng"],
    ["apply-loan", "card", "Nộp đơn vay mới"], ["user-settings", "settings", "Cài đặt & Bảo mật"],
  ];
  renderFeature(services.map(([page, iconName, label]) => `
    <button class="service-item" data-page="${page}"><span class="icon-box">${icon(iconName)}</span><b>${label}</b>${icon("arrow")}</button>`).join(""), "service-grid");
}

function renderFeature(content, className = "") {
  app.innerHTML = featureShell(`<section class="${className}">${content}</section>`);
}

function renderAccounts() {
  const accounts = state.accounts.map((a) => [escHtml(a.name), escHtml(a.number), vnd(a.primary ? state.balance : a.balance), a.primary]);
  if (!accounts.length) {
    renderFeature('<section class="surface"><h2 class="display">Tài khoản</h2><p class="product-note">Bạn chưa có tài khoản nào.</p></section>', "surface");
    return;
  }
  renderFeature(accounts.map(([name, number, balance, primary]) => `
    <article class="account-card ${primary ? "primary-account" : ""}">
      ${icon("wallet")}<p class="product-note">${name}</p><p class="product-value">${balance}</p><small>${number}</small>
    </article>`).join(""), "account-grid");
}

function renderCreditPage() {
  const account = state.creditAccount;
  const pendingApplication = state.creditApplications.find((item) => item.status === "PENDING");
  const documentStatus = (status) => ({ PENDING_REVIEW: "Đang kiểm tra giấy tờ", APPROVED: "Đã xác nhận giấy tờ", REJECTED: "Cần nộp lại giấy tờ" }[status] || "Chưa có giấy tờ");
  const apps = state.creditApplications.map((item) => `<div class="row"><div class="info"><p>Đề nghị ${vnd(Number(item.requested_limit_vnd))} · Thu nhập khai báo ${vnd(Number(item.monthly_income_vnd || 0))}/tháng</p><small>${escHtml(item.created_at || "")} · ${documentStatus(item.document_status)}${item.document_file_name ? ` · <a href="${escHtml(item.document_storage_key || "#")}" target="_blank" rel="noopener">${escHtml(item.document_file_name)}</a>` : ""}${item.document_review_note ? ` · ${escHtml(item.document_review_note)}` : ""}${item.decision_note ? ` · ${escHtml(item.decision_note)}` : ""}</small></div><b>${escHtml(item.status)}</b>${item.status === "PENDING" && (!item.document_status || item.document_status === "REJECTED") ? `<label class="btn-sm upload">${item.document_status === "REJECTED" ? "Nộp lại giấy tờ" : "Bổ sung giấy tờ"}<input type="file" accept=".pdf,.jpg,.jpeg,.png" data-credit-application="${escHtml(item.application_id)}" hidden /></label>` : ""}</div>`).join("");
  const accountContent = account ? `
    <section class="surface"><span class="eyebrow">HẠN MỨC TÍN DỤNG</span><h2 class="display">${vnd(account.limit_vnd)}</h2>
      <p>Dư nợ: <b>${vnd(account.used_vnd)}</b> · Còn khả dụng: <b>${vnd(account.available_vnd)}</b></p>
      <p class="product-note">Lãi suất ${account.monthly_rate_percent}%/tháng · Đến hạn ngày ${account.due_day} · Thanh toán tối thiểu hiện tại ${vnd(account.minimum_payment_vnd)}</p>
    </section>
    <div class="feature-layout"><section class="form-card"><h2 class="display">Chi tiêu hạn mức</h2><p class="product-note">Mỗi giao dịch tối đa 50.000.000 ₫.</p><form id="credit-spend-form">
      ${formField("credit-receiver", "Tài khoản nhận", "Nhập số tài khoản OOPBank", "user")}
      <p class="product-note">Hiện chỉ hỗ trợ chuyển tới tài khoản OOPBank; chuyển liên ngân hàng chưa được tích hợp.</p>
      ${formField("amount", "Số tiền", "VNĐ", "wallet", "number")}${formField("pin", "Mã PIN giao dịch", "6 chữ số", "key", "password")}
      <button class="primary" type="submit">Chi tiêu bằng tín dụng ${icon("arrow")}</button></form></section>
      <section class="form-card"><h2 class="display">Trả nợ từ tài khoản</h2><p class="product-note">Thanh toán tối thiểu gợi ý: ${vnd(account.minimum_payment_vnd)}. Có thể trả một phần hoặc toàn bộ dư nợ.</p><form id="credit-repay-form">
      <div class="field"><label for="credit-source-account">Tài khoản thanh toán</label><div class="input-wrap"><select id="credit-source-account" name="source_account_id" required><option value="">Chọn tài khoản nguồn</option>${(account.checking_accounts || []).map((source) => `<option value="${escHtml(source.account_id)}">${escHtml(source.account_id)} · Số dư ${vnd(source.balance_vnd)}</option>`).join("")}</select></div></div>
      ${formField("amount", "Số tiền trả", "VNĐ", "wallet", "number")}${formField("pin", "Mã PIN giao dịch", "6 chữ số", "key", "password")}
      <button class="primary" type="submit" ${(account.checking_accounts || []).length ? "" : "disabled"}>Thanh toán dư nợ ${icon("arrow")}</button></form></section></div>` : pendingApplication ? `
    <section class="form-card"><h2 class="display">Hồ sơ hạn mức đang được xem xét</h2><p class="product-note">Admin sẽ kiểm tra giấy tờ chứng minh thu nhập trước khi quyết định hạn mức.</p></section>` : `
    <section class="form-card"><h2 class="display">Xin cấp hạn mức</h2><p class="product-note">Hồ sơ gồm thu nhập khai báo và giấy tờ chứng minh thu nhập để Admin kiểm tra.</p><form id="credit-apply-form">
      ${formField("income", "Thu nhập tháng khai báo (VNĐ)", "Nhập thu nhập tháng", "wallet", "number")}
      ${formField("limit", "Hạn mức mong muốn (VNĐ)", "Tối đa 6 lần thu nhập, không quá 500.000.000", "wallet", "number")}
      <div class="field"><label for="credit-income-document">Giấy tờ chứng minh thu nhập</label><div class="input-wrap"><input id="credit-income-document" name="income_document" type="file" accept=".pdf,.jpg,.jpeg,.png" required /></div><small>PDF, JPG hoặc PNG · tối đa 5 MB</small></div>
      <p class="product-note">Admin quyết định hạn mức sau khi đối chiếu hồ sơ. Mức duyệt không vượt số tiền bạn đề nghị, 6 lần thu nhập tháng hoặc 500.000.000 đồng.</p>
      <button class="primary" type="submit">Gửi yêu cầu hạn mức ${icon("arrow")}</button></form></section>`;
  renderFeature(`<div class="stack"><button class="back" data-page="services">${icon("back")} Quay lại dịch vụ</button>${state.creditLoading ? '<p class="product-note">Đang tải dữ liệu tín dụng…</p>' : ""}${state.creditError ? `<p class="product-note">${escHtml(state.creditError)}</p>` : ""}${accountContent}<section class="surface"><h2 class="display">Hồ sơ hạn mức</h2>${apps || '<p class="product-note">Chưa có hồ sơ.</p>'}</section></div>`);
}

async function loadCreditPage() {
  state.creditLoading = true; state.creditError = ""; render();
  try {
    const [account, apps] = await Promise.all([
      callBackend("creditBridge", "getMyAccount", { session_token: state.sessionToken }).catch((error) => ({ success: false, message: error.message })),
      callBackend("creditBridge", "getApplications", { session_token: state.sessionToken }),
    ]);
    state.creditAccount = account.success ? account : null;
    state.creditError = account.success ? "" : account.message || "Chưa có hạn mức được duyệt.";
    state.creditApplications = apps.applications || [];
  } catch (error) { state.creditError = error?.message || String(error); }
  state.creditLoading = false; render();
}

async function uploadCreditDocument(file) {
  if (!file || !["application/pdf", "image/jpeg", "image/png"].includes(file.type)) {
    throw new Error("Chỉ nhận tệp PDF, JPG hoặc PNG.");
  }
  if (file.size <= 0 || file.size > 5 * 1024 * 1024) throw new Error("Tệp phải có dung lượng tối đa 5 MB.");
  const payload = new FormData();
  payload.append("file", file);
  payload.append("upload_preset", CLOUDINARY_UPLOAD_PRESET);
  const response = await fetch(`https://api.cloudinary.com/v1_1/${CLOUDINARY_CLOUD_NAME}/auto/upload`, { method: "POST", body: payload });
  const uploaded = await response.json();
  if (!response.ok || !uploaded.secure_url) throw new Error(uploaded.error?.message || "Không tải được giấy tờ lên kho lưu trữ.");
  return uploaded.secure_url;
}

const messages = [];
const seenNotificationIds = new Set();
let notificationBaselineReady = false;
let notificationPollRunning = false;

async function pollNotifications() {
  if (!state.loggedIn || state.role !== "CUSTOMER" || document.hidden || notificationPollRunning || !notificationBaselineReady) return;
  const bridge = getBridge("notificationBridge");
  if (!bridge || typeof bridge.getNotifications !== "function") return;
  notificationPollRunning = true;
  try {
    const response = await callBackend("notificationBridge", "getNotifications", { session_token: state.sessionToken });
    const rows = Array.isArray(response.notifications) ? response.notifications : [];
    const fresh = rows.filter((item) => item.notification_id && !seenNotificationIds.has(item.notification_id));
    fresh.forEach((item) => seenNotificationIds.add(item.notification_id));
    fresh.reverse().forEach((item) => {
      const notice = {
        id: item.notification_id,
        title: item.title || "Thông báo mới",
        preview: item.body || "",
        content: item.body || "",
        reference: item.reference_id || "",
        type: item.notification_type || "SYSTEM",
        icon: item.notification_type === "OTP" ? "shield"
          : item.notification_type === "BALANCE_CHANGE" ? "wallet"
          : item.notification_type === "TRANSACTION" ? (String(item.title || "").includes("nhận tiền") || String(item.title || "").startsWith("Tiền đã vào") ? "wallet" : "send")
          : item.notification_type === "CREDIT_BALANCE" ? "card" : "bell",
        unread: item.unread === true,
        time: item.created_at || "Vừa xong",
      };
      messages.unshift(notice);
      if (state.msgOpen) state.msgExpanded = notice;
      else showBanner(notice);
    });
    while (seenNotificationIds.size > 200) seenNotificationIds.delete(seenNotificationIds.values().next().value);
    if (fresh.length) {
      state.selectedMessage = 0;
      renderMsgbox();
      if (state.page === "notifications" && !/^(INPUT|TEXTAREA|SELECT)$/.test(document.activeElement?.tagName ?? "")) render();
    }
  } catch (_) {
    // A transient poll failure should not interrupt the banking flow; the inbox refresh retries it.
  } finally {
    notificationPollRunning = false;
  }
}

function renderNotifications() {
  if (!messages.length) {
    renderFeature('<section class="surface"><h2 class="display">Thông báo</h2><p class="product-note">Bạn chưa có thông báo nào.</p></section>', "surface");
    return;
  }
  if (state.selectedMessage >= messages.length) state.selectedMessage = 0;
  const message = messages[state.selectedMessage];
  const list = messages.map((item, index) => `
    <button class="message-item ${state.selectedMessage === index ? "active" : ""}" data-message="${index}">
      <span class="icon-box">${icon(item.icon)}</span>
      <span class="message-info"><strong>${escHtml(item.title)}</strong><span>${escHtml(item.preview)}</span></span>
      <time class="message-time">${escHtml(item.time)}</time>
    </button>`).join("");
  markRead(message);
  const otp = message.otp
    ? `<div class="otp">${message.otp.split("").map((number) => `<b>${number}</b>`).join("")}</div>${message.expiresAt ? otpTimer(message) : ""}` : "";
  renderFeature(`
    <section class="inbox">
      <div class="message-list"><div class="inbox-title"><h2 class="display">Hộp tin nhắn</h2><span class="badge">${messages.filter((m) => m.unread).length} chưa đọc</span></div>${list}</div>
      <article class="message-detail">
        <div class="message-detail-head"><span class="icon-box">${icon(message.icon)}</span><div><span class="eyebrow">OOPBank thông báo</span><h2 class="display">${escHtml(message.title)}</h2></div></div>
        <div class="message-body">${escHtml(message.content)}${otp}</div>
        <footer class="message-footer"><div><span class="eyebrow">Mã tham chiếu</span><br /><strong>${escHtml(message.reference)}</strong></div><button class="secondary" style="width:auto;margin:0">Đánh dấu đã đọc</button></footer>
      </article>
    </section>`);
}

function renderTransactions() {
  renderFeature(`<section class="surface"><h2 class="display">Lịch sử giao dịch</h2>${transactionRows(true)}</section>`, "surface");
}

function formField(id, label, placeholder, iconName = "card", type = "text") {
  return `<div class="field"><label for="${id}">${label}</label><div class="input-wrap">${icon(iconName)}<input id="${id}" name="${id}" type="${type}" placeholder="${placeholder}" required /></div></div>`;
}

// 4. TRANG CÀI ĐẶT HỒ SƠ VÀ BẢO MẬT (USER SERVICE)
function renderUserSettings() {
  const content = `
    <div class="grid-main">
      <section class="form-card">
        <h2 class="display">Cập nhật hồ sơ cá nhân</h2>
        <form id="update-profile-form">
          ${field("fullname", "Họ và tên", "user", "Họ và tên", "text", escHtml(state.profile.name))}
          ${field("email", "Địa chỉ Email", "mail", "email@example.com", "email", escHtml(state.profile.email))}
          ${field("phone", "Số điện thoại", "phone", "10 hoặc 11 chữ số", "tel", escHtml(state.profile.phone), 'inputmode="numeric" pattern="[0-9]{10,11}" maxlength="11" data-digits-only title="Số điện thoại chỉ gồm 10 hoặc 11 chữ số"')}
          <button class="primary" type="submit">Lưu thay đổi ${icon("check")}</button>
        </form>
      </section>

      <section class="form-card">
        <h2 class="display">Đổi mật khẩu & Mã PIN</h2>
        <form id="change-pass-form" class="form-stack">
          <span class="eyebrow">Mật khẩu đăng nhập</span>
          ${field("old_password", "Mật khẩu hiện tại", "lock", "••••••••", "password")}
          ${field("new_password", "Mật khẩu mới", "lock", "Tối thiểu 8 ký tự", "password")}
          <button class="secondary" type="submit">Cập nhật mật khẩu</button>
        </form>
        <hr class="divider" />
        <form id="change-pin-form" class="form-stack">
          <span class="eyebrow">Mã PIN giao dịch (6 số)</span>
          ${field("old_pin", "Mã PIN hiện tại", "key", "••••••", "password")}
          ${field("new_pin", "Mã PIN mới 6 số", "key", "••••••", "password")}
          <button class="secondary" type="submit">Đổi mã PIN</button>
        </form>
      </section>
    </div>`;
  renderFeature(content);
}

function renderTransactionForm() {
  if (state.transactionSuccess) {
    const isLoanApplication = state.page === "apply-loan";
    const successMessage = state.transactionSuccessMessage || (isLoanApplication
      ? "Hồ sơ đã được lưu. Tiếp tục nộp giấy tờ để ngân hàng thẩm định."
      : "Yêu cầu của bạn đã được ghi nhận và xử lý thành công.");
    const content = `
      <div class="success"><span class="success-icon">${icon("check")}</span><h2 class="display">${isLoanApplication ? "Đã tiếp nhận hồ sơ vay" : "Đã ghi nhận yêu cầu"}</h2><p>${escHtml(successMessage)}</p><button class="primary" data-page="home">Về trang chủ</button>${isLoanApplication ? '<div class="loan-actions"><button class="primary" data-loan-tab="docs">Nộp giấy tờ</button><button class="secondary" data-loan-tab="review">Theo dõi hồ sơ</button></div>' : ""}</div>`;
    app.innerHTML = featureShell(`<div class="feature-layout"><section class="form-card">${content}</section>${sourceCard()}</div>`);
    return;
  }

  let fields = "";
  let heading = "";

  if (state.page === "transfer") {
    heading = "Thông tin người nhận";
    fields = formField("receiver", "Số tài khoản người nhận", "Nhập 8–16 số", "user") + `<div class="field"><label for="bank">Ngân hàng thụ hưởng</label><div class="input-wrap"><select id="bank" name="bank"><option value="OOPBANK">OOPBank nội bộ</option><option value="MB">MB Bank</option><option value="AGRIBANK">Agribank</option><option value="VIETCOMBANK">Vietcombank</option><option value="BIDV">BIDV</option><option value="TECHCOMBANK">Techcombank</option><option value="ACB">ACB</option></select></div></div><p class="product-note">Chuyển ngoài OOPBank được ghi nhận trong database ở chế độ mô phỏng; chưa gửi qua ngân hàng liên kết.</p>`;
  } else if (state.page === "topup") {
    heading = "Thông tin thuê bao";
    fields = `<div class="field"><label for="carrier">Nhà mạng</label><div class="input-wrap"><select id="carrier" name="carrier"><option value="VIETTEL">Viettel</option><option value="MOBIFONE">MobiFone</option><option value="VINAPHONE">VinaPhone</option></select></div></div>` + formField("topup-phone", "Số điện thoại", "0912345678", "phone", "tel") + `<p class="product-note">Giao dịch được ghi nhận trong database; hiện chưa gửi yêu cầu qua hệ thống nhà mạng.</p>`;
  } else if (state.page === "payment") {
    heading = "Thanh toán hóa đơn";
    fields = billSelector() + providerSelector() + formField("bill-code", "Mã khách hàng / Hóa đơn", "Nhập mã tra cứu", "card") + `<p class="product-note">Tra cứu và thanh toán hóa đơn hiện mô phỏng bằng dữ liệu trong OOPBank; chưa kết nối nhà cung cấp và không gửi xác nhận thanh toán ra ngoài.</p>`;
  } 
  // 2. FORM MỞ SỔ TIẾT KIỆM (KÈM PHƯƠNG THỨC ĐÁO HẠN & TẤT TOÁN)
  else if (state.page === "savings") {
    heading = "Mở sổ tiết kiệm mới";
    fields = formField("saving-amount", "Số tiền gửi (VNĐ)", "Tối thiểu 1.000.000", "wallet", "number") + `
      <div class="field">
        <label for="term">Kỳ hạn gửi</label>
        <div class="input-wrap"><select id="term" name="term"><option value="3">3 tháng · 4,5%/năm</option><option value="6" selected>6 tháng · 5,8%/năm</option><option value="12">12 tháng · 6,1%/năm</option></select></div>
      </div>
      <div class="field">
        <label for="maturity-option">Phương thức đáo hạn</label>
        <div class="input-wrap">
          <select id="maturity-option" name="maturity_option">
            <option value="RENEW_ALL">Tự động tái tục cả gốc và lãi (RENEW_ALL)</option>
            <option value="RENEW_PRINCIPAL">Tái tục gốc, lãi chuyển về TK thanh toán (RENEW_PRINCIPAL)</option>
            <option value="CLOSE_TO_ACCOUNT">Tất toán toàn bộ về TK thanh toán khi hết hạn (CLOSE_TO_ACCOUNT)</option>
          </select>
        </div>
      </div>`;
  } 
  // 3. FORM NỘP ĐƠN XIN VAY (APPLY LOAN)
  else if (state.page === "apply-loan") {
    heading = "Đăng ký khoản vay mới";
    fields = formField("loan-amount", "Số tiền cần vay (VNĐ)", "Ví dụ: 50000000", "wallet", "number") + `
      <div class="field">
        <label for="loan-term">Kỳ hạn vay (tháng)</label>
        <div class="input-wrap"><select id="loan-term" name="loan_term">${termOptions()}</select></div>
      </div>
      <div class="field">
        <label for="calc-method">Phương thức trả nợ</label>
        <div class="input-wrap">
          <select id="calc-method" name="calc_method">
            <option value="DECREASING">Dư nợ giảm dần</option>
            <option value="FIXED">Niên kim cố định (Gốc + Lãi chia đều)</option>
          </select>
        </div>
      </div>` + formField("loan-purpose", "Mục đích vay", "Ví dụ: học phí, sửa nhà", "file") +
      formField("monthly-income", "Thu nhập hàng tháng (VNĐ)", "Ví dụ: 18000000", "wallet", "number") +
      formField("collateral", "Thông tin tài sản đảm bảo", "Ví dụ: bảng lương / giấy tờ xe / bất động sản", "shield");
  } else {
    heading = "Thanh toán kỳ vay";
  }

  const needsAmount = !["payment", "savings", "apply-loan"].includes(state.page);
  const amount = needsAmount ? (state.page === "topup" ? `<div class="field"><label for="amount">Mệnh giá</label><div class="input-wrap"><select id="amount" name="amount"><option value="10000">10.000 ₫</option><option value="20000">20.000 ₫</option><option value="50000">50.000 ₫</option><option value="100000">100.000 ₫</option><option value="200000">200.000 ₫</option><option value="500000">500.000 ₫</option></select></div></div>` : formField("amount", state.page === "loan-pay" ? "Số tiền thanh toán" : "Số tiền (VNĐ)", "0 VND", "wallet", "number")) : "";

  // Bổ sung nút tất toán sổ tiết kiệm trực tiếp
  const closeSavingsAction = state.page === "savings" ? `
    <div class="close-savings-box">
      <hr />
      <span class="eyebrow">Sổ tiết kiệm đang hoạt động</span>
      ${state.savings.filter((item) => item.status === "ACTIVE").map((item) => `
        <div class="row">
          <div class="info"><p>${escHtml(item.savings_id)}</p><small>${vnd(item.principal_vnd)} · ${item.term_months} tháng · ${item.interest_rate}%/năm</small></div>
          <button class="secondary" type="button" data-action="close-savings-request" data-savings-id="${escHtml(item.savings_id)}">Tất toán</button>
        </div>`).join("") || `<p class="product-note">Chưa có sổ tiết kiệm đang hoạt động.</p>`}
    </div>` : "";

  const content = `
    ${state.page === "apply-loan" ? loanTabs() : ""}
    <div class="feature-layout">
      <section class="form-card">
        <h2 class="display">${heading}</h2>
        <form id="transaction-form">
          ${fields}${amount}
          <button class="primary" type="submit">Tiếp tục xác thực ${icon("arrow")}</button>
        </form>
        ${closeSavingsAction}
      </section>
      ${sourceCard()}
    </div>`;
  app.innerHTML = featureShell(content);
}

function billSelector() {
  return `
    <div class="field"><span class="field-label">Chọn loại hóa đơn</span>
      <div class="bill-types">
        ${billButton("electricity", "plus", "Điện")}
        ${billButton("water", "wallet", "Nước")}
        ${billButton("internet", "grid", "Internet")}
        ${billButton("mobile", "phone", "Điện thoại trả sau")}
      </div>
    </div>`;
}

function billButton(type, iconName, label) {
  return `<button class="bill-type ${state.billType === type ? "active" : ""}" type="button" data-bill="${type}">${icon(iconName)}${label}</button>`;
}

function providersForBillType() {
  const serviceType = { electricity: "ELECTRICITY", water: "WATER", internet: "INTERNET", mobile: "MOBILE_POSTPAID" }[state.billType];
  return state.billProviders.filter((provider) => String(provider.service_type).toUpperCase() === serviceType);
}

function providerSelector() {
  const fallback = {
    electricity: ["EVN Hà Nội", "EVN TP. Hồ Chí Minh", "EVN Miền Trung"],
    water: ["Nước sạch Hà Nội", "SAWACO", "Cấp nước Đà Nẵng"],
    internet: ["Viettel Internet", "VNPT", "FPT Telecom"],
    mobile: ["Viettel trả sau", "MobiFone trả sau", "VinaPhone trả sau"],
  };
  const options = hasBackend()
    ? providersForBillType().map((provider) => `<option value="${escHtml(provider.provider_id)}">${escHtml(provider.provider_name)}</option>`).join("")
    : fallback[state.billType].map((name) => `<option value="${escHtml(name)}">${escHtml(name)}</option>`).join("");
  const placeholder = hasBackend() && !state.billProvidersLoaded
    ? "<option value=''>Đang tải nhà cung cấp…</option>"
    : hasBackend() && !options
      ? `<option value="">${state.billProvidersError ? "Không tải được nhà cung cấp" : "Database chưa có nhà cung cấp cho loại hóa đơn này"}</option>`
      : options;
  return `
    <div class="field"><label for="provider">Nhà cung cấp</label><div class="input-wrap">
      <select id="provider" name="provider" ${hasBackend() && !options ? "disabled" : ""}>${placeholder}</select>
    </div></div>`;
}

async function loadBillProviders() {
  const result = await callBackend("billBridge", "getProviders", { session_token: state.sessionToken });
  state.billProviders = result.providers || [];
  state.billProvidersLoaded = true;
  state.billProvidersError = "";
  if (state.page === "payment") {
    const customerCode = document.querySelector("#bill-code")?.value || "";
    render();
    const codeInput = document.querySelector("#bill-code");
    if (codeInput) codeInput.value = customerCode;
  }
}

function ensureBillProviders() {
  if (state.page !== "payment" || !hasBackend() || state.billProvidersLoaded) return;
  state.billProvidersLoaded = true;
  loadBillProviders().catch((error) => {
    state.billProvidersError = error?.message || String(error);
    if (state.page === "payment") {
      const customerCode = document.querySelector("#bill-code")?.value || "";
      render();
      const codeInput = document.querySelector("#bill-code");
      if (codeInput) codeInput.value = customerCode;
    }
    showToast(`Không tải được nhà cung cấp hóa đơn: ${state.billProvidersError}`);
  });
}

function sourceCard() {
  return `<aside class="source-card"><span class="eyebrow">Nguồn tiền</span><p>Tài khoản thanh toán</p><b>0123 6868 8888</b><hr /><p>Số dư khả dụng</p><strong>${vnd(state.balance)}</strong></aside>`;
}

// ===== HỘP THƯ NỔI (góc dưới màn hình): nhận tiền · chuyển tiền · mã OTP =====
const msgHost = document.createElement("div");
msgHost.id = "msgbox";
document.body.append(msgHost);

const makeRef = () => `FT${Date.now().toString().slice(-14)}`;
const stampShort = () => { const d = new Date(); return `${pad(d.getDate())}/${pad(d.getMonth() + 1)}, ${nowHM()}`; };
const timerText = (exp) => { const r = Math.ceil((exp - Date.now()) / 1000); return r <= 0 ? "Mã đã hết hạn" : `Hiệu lực còn ${Math.floor(r / 60)}:${pad(r % 60)}`; };
const otpTimer = (m) => `<small class="otp-timer" data-exp="${m.expiresAt}">${timerText(m.expiresAt)}</small>`;
const msgIcon = (m) => icon(m.icon || "bell");

let bannerTimer;
function showBanner(m) {
  state.msgBanner = m;
  clearTimeout(bannerTimer);
  bannerTimer = setTimeout(() => { state.msgBanner = null; renderMsgbox(); }, 4500);
  bannerTimer?.unref?.();
}

function pushMessage(m) {
  messages.unshift({ unread: true, time: "Vừa xong", ...m });
  state.selectedMessage = 0;
  if (state.msgOpen) state.msgExpanded = messages[0];
  else showBanner(messages[0]);
  renderMsgbox();
}

function recordPayment(tx) {
  const to = escHtml(tx.data?.receiver || tx.data?.["topup-phone"] || "");
  state.balance -= tx.amount;
  if (tx.type === "transfer") {
    transactions.unshift([`Chuyển đến TK ${to}`, `Chuyển tiền · ${stampShort()}`, `-${vnd(tx.amount)}`, "send"]);
    pushMessage({ title: "Chuyển tiền thành công", preview: `-${vnd(tx.amount)} đến TK ${to}`, icon: "send", content: `Bạn đã chuyển thành công ${vnd(tx.amount)} đến tài khoản ${to}. Số dư khả dụng: ${vnd(state.balance)}.`, reference: makeRef() });
  } else {
    transactions.unshift([`Nạp tiền ${to}`, `Nạp tiền · ${stampShort()}`, `-${vnd(tx.amount)}`, "phone"]);
    pushMessage({ title: "Nạp tiền điện thoại thành công", preview: `-${vnd(tx.amount)} · ${to}`, icon: "phone", content: `Bạn đã nạp ${vnd(tx.amount)} cho số ${to}. Số dư khả dụng: ${vnd(state.balance)}.`, reference: makeRef() });
  }
}

async function maybeSendOtp() {
  const tx = state.pendingTransaction;
  if (!tx || tx.type !== "transfer" || tx.amount <= 50000000) return;
  try {
    const challenge = await callBackend("transactionBridge", "requestOtp", {
      session_token: state.sessionToken,
      purpose: "TRANSFER",
      idempotency_key: tx.idempotencyKey,
      receiver_account_id: tx.data.bank && tx.data.bank !== "OOPBANK" ? `EXT:${tx.data.bank}:${String(tx.data.receiver || "").trim()}` : String(tx.data.receiver || "").trim(),
      amount_vnd: tx.amount,
    });
    if (!challenge?.success || !challenge.challenge_id || !challenge.otp_code) {
      throw new Error(challenge?.message || "Không tạo được mã xác nhận.");
    }
    tx.otpChallengeId = challenge.challenge_id;
    tx.otpCode = challenge.otp_code;
    render();
  } catch (error) {
    state.pendingTransaction = null;
    state.showPinModal = false;
    render();
    showToast("Không tạo được mã xác nhận; giao dịch chưa được tiếp tục: " + (error?.message || String(error)));
  }
}

function renderMsgbox() {
  if (!state.loggedIn || state.role !== "CUSTOMER") { msgHost.innerHTML = ""; return; }
  const unread = messages.filter((m) => m.unread).length;
  const detail = (m) => `<div class="msg-detail"><p>${m.content}</p>${m.otp ? `<div class="otp">${m.otp.split("").map((n) => `<b>${n}</b>`).join("")}</div>${m.expiresAt ? otpTimer(m) : ""}` : ""}<small>Mã tham chiếu: ${m.reference}</small></div>`;
  const items = messages.map((m) => `
    <div class="msg-entry ${m.unread ? "unread" : ""}">
      <button class="msg-item" data-msg-open="${messages.indexOf(m)}" aria-expanded="${state.msgExpanded === m}">
        <span class="icon-box">${msgIcon(m)}</span>
        <span class="msg-text"><strong>${m.title}</strong><span>${m.preview}</span></span>
        <time>${m.time}</time>
      </button>${state.msgExpanded === m ? detail(m) : ""}
    </div>`).join("");
  const banner = state.msgBanner && !state.msgOpen
    ? `<button class="msg-banner" data-msg="openpanel"><span class="icon-box">${msgIcon(state.msgBanner)}</span><span class="msg-text"><strong>${state.msgBanner.title}</strong><span>${state.msgBanner.preview}</span></span></button>` : "";
  const panel = state.msgOpen ? `
    <section class="msg-panel" role="dialog" aria-label="Hộp thư OOPBank">
      <header><div><b>Hộp thư OOPBank</b><small>${unread ? `${unread} tin chưa đọc` : "Đã đọc tất cả"}</small></div>
        <div class="msg-head-actions"><button class="msg-link" data-msg="readall">Đọc tất cả</button><button class="msg-close" data-msg="toggle" aria-label="Đóng hộp thư">×</button></div></header>
      <div class="msg-list">${items}</div>
      <footer><button class="msg-link" data-page="notifications">Xem tất cả thông báo</button></footer>
    </section>` : "";
  const list = msgHost.querySelector(".msg-list");
  const keep = list ? list.scrollTop : 0;
  msgHost.innerHTML = `${banner}${panel}<button class="msg-fab" data-msg="toggle" aria-label="Hộp thư${unread ? `, ${unread} tin chưa đọc` : ""}" aria-expanded="${state.msgOpen}">${icon("mail")}${unread ? `<i class="msg-count">${unread}</i>` : ""}</button>`;
  const fresh = msgHost.querySelector(".msg-list");
  if (fresh) fresh.scrollTop = keep;
}

msgHost.addEventListener("click", (event) => {
  const t = event.target;
  const item = t.closest("[data-msg-open]");
  const nav = t.closest("[data-page]");
  const act = t.closest("[data-msg]")?.dataset.msg;
  if (item) {
    const m = messages[Number(item.dataset.msgOpen)];
    state.msgExpanded = state.msgExpanded === m ? null : m;
    markRead(m);
    return render();
  }
  if (nav) {
    state.page = nav.dataset.page;
    state.transactionSuccess = false;
    state.msgOpen = false;
    window.scrollTo({ top: 0, behavior: "smooth" });
    render();
    ensureBillProviders();
    return loadCurrent();
  }
  if (act === "toggle") { state.msgOpen = !state.msgOpen; state.msgBanner = null; }
  else if (act === "openpanel") { state.msgOpen = true; state.msgBanner = null; }
  else if (act === "readall") messages.forEach(markRead);
  render();
});

// đồng hồ đếm ngược hiệu lực OTP
const otpClock = setInterval(() => {
  document.querySelectorAll("[data-exp]").forEach((el) => { el.textContent = timerText(Number(el.dataset.exp)); });
}, 1000);
otpClock?.unref?.();

// ===== VAY VỐN: đăng ký · bổ sung hồ sơ · xử lý · giải ngân · quản lý =====
const vnd = (n) => `${Number(n).toLocaleString("vi-VN")} ₫`;
const pad = (n) => String(n).padStart(2, "0");
const fmtDate = (iso) => iso.split("-").reverse().join("/");
const nowHM = () => new Date().toTimeString().slice(0, 5);
const escHtml = (v) => String(v).replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));

function showToast(message) {
  const el = document.createElement("div");
  el.className = "toast";
  el.textContent = message;
  document.body.append(el);
  setTimeout(() => el.remove(), 2400);
}

// Cấu hình kỳ hạn vay (Admin thiết lập ở mục Cấu hình): chỉ các kỳ hạn đang bật mới hiện ở form đăng ký vay
const loanTerms = [];
const termRate = (months) => loanTerms.find((t) => t.months === months)?.rate ?? 0;
const termOptions = () => {
  const on = loanTerms.filter((t) => t.enabled);
  return on.map((t, i) => `<option value="${t.months}" ${i === on.length - 1 ? "selected" : ""}>${t.months} tháng · ${t.rate}%/năm</option>`).join("");
};
const LOAN_WORKFLOW_STEPS = ["Nộp đơn", "Nộp hồ sơ", "Kiểm tra giấy tờ", "Thẩm định & phê duyệt", "Giải ngân", "Trả nợ"];
const LOAN_TABS = [["apply", "1 · Đăng ký"], ["docs", "2 · Giấy tờ"], ["review", "3 · Thẩm định"], ["disburse", "4 · Giải ngân"], ["manage", "5 · Trả nợ"]];
const DOC_STATUS = { DONE: ["Đã xác nhận", "ok"], REVIEW: ["Đang kiểm tra", "role"], MISSING: ["Chưa nộp", "warn"], RESUBMIT: ["Cần nộp lại", "warn"] };
const NOTICE = {
  APPROVED: ["ok", "Hồ sơ đã được phê duyệt"],
  REJECTED: ["warn", "Hồ sơ bị từ chối"],
  INFO: ["warn", "Ngân hàng yêu cầu bổ sung hồ sơ"],
  SUBMITTED: ["role", "Đã gửi bổ sung hồ sơ"],
  DISBURSED: ["ok", "Khoản vay đã được giải ngân"],
};

// Bộ giấy tờ chuẩn; statuses theo thứ tự: CCCD, thu nhập, tài sản đảm bảo, cư trú
const DOC_BASE = [
  ["cccd", "shield", "CCCD gắn chip (2 mặt)"],
  ["income", "wallet", "Sao kê lương / HĐLĐ 3 tháng gần nhất"],
  ["collateral", "file", "Giấy tờ tài sản đảm bảo"],
  ["address", "home", "Xác nhận nơi cư trú"],
];
const loanDocumentName = (type) => {
  const value = String(type || "");
  if (value.startsWith("CUSTOM:")) return value.slice(7);
  return DOC_BASE.find(([id]) => id === value)?.[2] || value;
};
const loanApps = [];
const activeLoans = [];

const mine = () => loanApps.filter((a) => a.who === state.profile.name);
const needsUpload = (doc) => doc.status === "MISSING" || doc.status === "RESUBMIT";
const missingDocs = (a) => a.docs.filter(needsUpload).length;
const openApps = () => mine().filter((a) => a.stage >= 0 && a.stage < 2);
const docApp = () => { const list = openApps(); return list.find((a) => a.id === state.docAppId) || list[0]; };
const monthlyPay = (a) => Math.round(a.amt / a.term + (a.amt * a.rate) / 1200);
const appChip = (a) => {
  if (a.stage < 0 || a.status === "REJECTED") return ["Bị từ chối", "warn"];
  if (a.status === "PAID" || a.status === "CLOSED") return ["Đã tất toán", "ok"];
  if (a.status === "DISBURSED" || a.stage >= 4) return ["Đang trả nợ", "ok"];
  if (a.status === "APPROVED" || a.stage === 3) return ["Chờ giải ngân", "ok"];
  if (a.needInfo || a.docs.some(needsUpload)) return ["Cần bổ sung giấy tờ", "warn"];
  if (a.docs.some((d) => d.status === "REVIEW")) return ["Đang kiểm tra giấy tờ", "role"];
  return ["Đang thẩm định", "role"];
};
function loanWorkflowStage(a) {
  if (a.status === "PAID" || a.status === "CLOSED") return LOAN_WORKFLOW_STEPS.length;
  if (a.status === "DISBURSED" || a.stage >= 4) return 5;
  if (a.status === "APPROVED" || a.stage === 3) return 4;
  if (a.stage < 0 || a.status === "REJECTED") return -1;
  if (!a.docs.length || a.docs.some(needsUpload)) return 1;
  if (a.docs.some((d) => d.status === "REVIEW")) return 2;
  return 3;
}
function loanWorkflowTimeline(a) {
  const current = loanWorkflowStage(a);
  if (current < 0) return "";
  return `<ol class="timeline loan-steps" aria-label="Tiến độ hồ sơ vay">${LOAN_WORKFLOW_STEPS.map((step, index) => {
    const stateClass = index < current ? "done" : index === current ? "current" : "";
    return `<li class="${stateClass}" ${index === current ? 'aria-current="step"' : ""}><span class="dot">${index < current ? icon("check") : index + 1}</span><b>${step}</b></li>`;
  }).join("")}</ol>`;
}
function loanWorkflowMessage(a) {
  if (a.status === "PAID" || a.status === "CLOSED") return "Khoản vay đã được tất toán.";
  if (a.status === "DISBURSED" || a.stage >= 4) return "Khoản vay đã được giải ngân. Bạn có thể theo dõi lịch và thanh toán từng kỳ ở mục Trả nợ.";
  if (a.status === "APPROVED" || a.stage === 3) return "Hồ sơ đã được phê duyệt. Admin sẽ thực hiện giải ngân vào tài khoản thanh toán của bạn.";
  if (a.stage < 0 || a.status === "REJECTED") return a.result?.note || "Hồ sơ chưa được phê duyệt. Xem thông báo để biết lý do.";
  if (a.docs.some(needsUpload)) return "Hãy nộp hoặc nộp lại các giấy tờ còn thiếu để ngân hàng tiếp tục thẩm định.";
  if (a.docs.some((d) => d.status === "REVIEW")) return "Ngân hàng đang kiểm tra giấy tờ bạn đã gửi.";
  return "Giấy tờ đã được xác nhận. Ngân hàng đang thẩm định hồ sơ vay.";
}
const emptyCard = (text) => `<section class="form-card"><p class="product-note">${text}</p></section>`;

function noticeHtml(a) {
  if (!a.result) return "";
  const [cls, title] = NOTICE[a.result.type];
  return `<div class="notice ${cls}"><b>${title}</b><p>${escHtml(a.result.note)}</p><small>${a.result.by ? `${escHtml(a.result.by)} · ` : ""}${a.result.time}</small></div>`;
}

// Gửi kết quả / yêu cầu của GDV-Admin về hộp thông báo của khách hàng
function notifyCustomer(a, kind, note) {
  if (a.who !== state.profile.name) return;
  pushMessage({ title: NOTICE[kind][1], preview: `${a.id} · ${vnd(a.amt)}`, icon: kind === "APPROVED" ? "check" : "bell", content: `${escHtml(note)} (Khoản vay ${a.id} · ${vnd(a.amt)}).`, reference: a.id });
}

function loanTabs() {
  const badge = {
    docs: openApps().reduce((n, a) => n + missingDocs(a), 0),
    disburse: mine().filter((a) => a.stage === 3).length,
  };
  return `<div class="loan-tabs" role="tablist">${LOAN_TABS.map(([key, label]) => `
    <button class="tab ${state.loanTab === key ? "active" : ""}" role="tab" aria-selected="${state.loanTab === key}" data-loan-tab="${key}">${label}${badge[key] ? `<i class="tab-badge">${badge[key]}</i>` : ""}</button>`).join("")}</div>`;
}

function loanDocsView() {
  const target = docApp();
  if (!target) return emptyCard("Hiện không có hồ sơ nào cần bổ sung giấy tờ.");
  const list = openApps();
  const picker = list.length > 1 ? `<div class="loan-tabs sub">${list.map((a) => `<button class="tab ${a.id === target.id ? "active" : ""}" data-doc-app="${a.id}">${a.id}</button>`).join("")}</div>` : "";
  const missing = missingDocs(target);
  const notice = target.result?.type === "INFO" ? noticeHtml(target) : "";
  const rows = target.docs.map((d) => {
    const [text, cls] = DOC_STATUS[d.status];
    const upload = d.status === "DONE" ? "" : `<label class="btn-sm upload">${d.status === "RESUBMIT" ? "Nộp lại" : d.status === "MISSING" ? "Tải lên" : "Thay tệp"}<input type="file" accept=".pdf,.jpg,.jpeg,.png" data-doc="${d.id}" data-loan="${target.id}" hidden /></label>`;
    const reviewHint = needsUpload(d) && d.hint && d.hint !== "Chưa có tệp"
      ? `<small class="doc-review-note">Lý do bổ sung: ${escHtml(d.hint)}</small>` : "";
    return `<div class="row"><span class="icon-box">${icon(d.icon)}</span>
      <div class="info"><p>${escHtml(d.name || loanDocumentName(d.id))}</p><small>${d.file ? escHtml(d.file) : escHtml(d.hint || "Chưa có tệp")}</small>${reviewHint}</div>
      <span class="chip ${cls}">${text}</span>${upload}</div>`;
  }).join("");
  const approved = target.docs.filter((d) => d.status === "DONE").length;
  const inReview = target.docs.filter((d) => d.status === "REVIEW").length;
  return `
    ${picker}${notice}
    <div class="feature-layout">
      <section class="form-card">
        <h2 class="display">Bổ sung hồ sơ vay</h2>
        <p class="product-note">Hồ sơ ${target.id} · ${vnd(target.amt)} · ${target.term} tháng</p>
        <p class="notice warn">Nộp từng giấy tờ bên dưới. Tệp ở trạng thái “Đang kiểm tra” cần được ngân hàng xác nhận; nếu cần sửa, ngân hàng sẽ ghi rõ lý do tại đây.</p>
        <div class="doc-list">${rows}</div>
      </section>
      <aside class="source-card"><span class="eyebrow">Tiến độ giấy tờ</span><p>Đã xác nhận</p><strong>${approved}/${target.docs.length} giấy tờ</strong><hr /><p>${inReview} đang được kiểm tra · ${missing} cần nộp hoặc nộp lại.</p><p>PDF, JPG hoặc PNG · tối đa 5 MB mỗi tệp. Từng tệp sẽ được gửi để ngân hàng kiểm tra.</p></aside>
    </div>`;
}

function loanReviewView() {
  const apps = mine().filter((a) => a.stage < 4 || ["PAID", "CLOSED"].includes(a.status));
  if (!apps.length) return emptyCard("Bạn chưa có hồ sơ vay nào đang xử lý.");
  return `<div class="stack">${apps.map((a) => {
    const [ct, cc] = appChip(a);
    const missing = missingDocs(a);
    const isApproved = a.status === "APPROVED" || a.stage === 3;
    const isDisbursed = a.status === "DISBURSED" || (a.stage >= 4 && !["PAID", "CLOSED"].includes(a.status));
    const action = isApproved
      ? '<button class="btn-sm" data-loan-tab="disburse">Theo dõi giải ngân</button>'
      : isDisbursed ? '<button class="btn-sm" data-loan-tab="manage">Theo dõi kỳ trả nợ</button>'
      : a.stage >= 0 && missing ? `<button class="btn-sm" data-loan-tab="docs" data-doc-app="${escHtml(a.id)}">Bổ sung hồ sơ (${missing})</button>` : "";
    return `<section class="form-card">
      <div class="loan-head"><div><span class="eyebrow">${a.id}</span><h2 class="display">${vnd(a.amt)}</h2><p class="product-note">${a.type} · ${a.term} tháng</p></div><span class="chip ${cc}">${ct}</span></div>
      ${noticeHtml(a)}${loanWorkflowTimeline(a)}
      <p class="product-note">${escHtml(loanWorkflowMessage(a))}</p>${action ? `<div class="loan-actions">${action}</div>` : ""}
    </section>`;
  }).join("")}</div>`;
}

function loanDisburseView() {
  const ready = mine().filter((a) => a.stage === 3);
  if (!ready.length) return emptyCard("Chưa có khoản vay nào đủ điều kiện giải ngân. Khoản vay sẽ xuất hiện tại đây sau khi được phê duyệt.");
  const cards = ready.map((a) => `
    <section class="form-card">
      <div class="loan-head"><div><span class="eyebrow">${escHtml(a.id)}</span><h2 class="display">${vnd(a.amt)}</h2><p class="product-note">${escHtml(a.type)} · ${a.term} tháng</p></div><span class="chip ok">Đã phê duyệt</span></div>
      <div class="loan-facts">
        <div><small>Kỳ hạn</small><b>${a.term} tháng</b></div>
        <div><small>Lãi suất</small><b>${a.rate}%/năm</b></div>
        <div><small>Trả hàng tháng (ước tính)</small><b>${vnd(monthlyPay(a))}</b></div>
        <div><small>Tài khoản nhận</small><b>Tài khoản thanh toán đang hoạt động</b></div>
      </div>
      <p class="notice">Hồ sơ đã được duyệt. Admin sẽ kiểm tra và giải ngân vào tài khoản thanh toán của bạn; bạn không cần xác nhận tại màn hình này.</p>
    </section>`).join("");
  return `<div class="stack">${cards}</div>`;
}

function scheduleRows(l) {
  const rows = (l.schedules || []).filter((item) => item.status !== "PAID").slice(0, 4);
  if (!(l.schedules || []).length && l.left > 0 && hasBackend()) return `<p class="product-note">Chưa có lịch trả nợ trong database.</p>`;
  if (!rows.length) return `<p class="product-note">Không còn kỳ thanh toán.</p>`;
  return rows.map((item) => `<div class="row"><span class="icon-box">${icon("clock")}</span><div class="info"><p>Kỳ ${item.period_num}/${l.total}${item.status === "OVERDUE" ? " · Quá hạn" : ""}</p><small>${fmtDate(item.due_date)}</small></div><b class="amount">${vnd(item.total_due_vnd)}</b></div>`).join("");
}

function loanManageView() {
  const loans = activeLoans.map((l) => {
    const pct = l.total ? Math.round(((l.total - l.left) / l.total) * 100) : 0;
    const open = state.openSchedule === l.id;
    const canPay = Boolean(l.nextScheduleId && l.left > 0);
    const payLabel = canPay ? "Thanh toán kỳ này" : l.left > 0 ? "Chưa có lịch trả nợ" : "Đã tất toán";
    return `<section class="surface">
      <div class="product-head"><div class="product-title"><span class="icon-box">${icon("card")}</span><div><span class="eyebrow">${l.id}</span><h3>${l.type}</h3></div></div><span class="status">Còn ${l.left} kỳ</span></div>
      <p class="product-value">${vnd(l.outstanding)}</p>
      <p class="product-note">Dư nợ gốc còn lại · Gốc ban đầu ${vnd(l.principal)} · Lãi ${l.rate}%/năm</p>
      <div class="progress"><i style="width:${pct}%"></i></div>
      <p class="product-note">Đã trả ${l.total - l.left}/${l.total} kỳ · Kỳ tới ${vnd(l.pay)} ngày ${fmtDate(l.due)}</p>
      <div class="loan-actions"><button class="primary" data-loan-pay="${escHtml(l.nextScheduleId || "")}" data-loan-id="${escHtml(l.id)}" data-loan-amount="${Number(l.pay || 0)}" data-loan-account="${escHtml(l.accountId || "")}" ${canPay ? "" : "disabled"}>${payLabel}</button><button class="secondary" data-action="toggle-schedule" data-id="${escHtml(l.id)}">${open ? "Ẩn" : "Xem"} lịch trả nợ</button></div>
      ${open ? `<div class="schedule">${scheduleRows(l)}</div>` : ""}
    </section>`;
  }).join("");
  const pending = mine().filter((a) => a.stage < 4).map((a) => {
    const [ct, cc] = appChip(a);
    return `<div class="row"><span class="icon-box">${icon("file")}</span>
      <div class="info"><p>${vnd(a.amt)} · ${a.term} tháng</p><small>${a.id} · ${a.type}</small></div>
      <span class="chip ${cc}">${ct}</span>
      <button class="btn-sm" data-loan-tab="review">Theo dõi</button></div>`;
  }).join("");
  return `<div class="stack">
    ${loans || emptyCard("Bạn chưa có khoản vay nào đang hoạt động.")}
    <section class="surface">
      <div class="surface-title"><h2 class="display">Đơn vay đang xử lý</h2><button class="text-action" data-loan-tab="apply">Đăng ký vay mới</button></div>
      ${pending || '<p class="product-note">Không có đơn vay nào đang xử lý.</p>'}
    </section>
  </div>`;
}

function renderLoanHub() {
  const views = { docs: loanDocsView, review: loanReviewView, disburse: loanDisburseView, manage: loanManageView };
  app.innerHTML = featureShell(`${loanTabs()}${views[state.loanTab]()}`);
}

// Thông tin khoản vay hiển thị trong hộp xác thực trước khi khách thực hiện giao dịch
function pendingSummary() {
  const tx = state.pendingTransaction;
  if (!tx || tx.type !== "apply-loan") return "";
  const term = Number(tx.data?.loan_term) || 12;
  const rate = termRate(term);
  const monthly = Math.round(tx.amount / term + (tx.amount * rate) / 1200);
  return `<div class="loan-facts modal-facts">
    <div><small>Số tiền</small><b>${vnd(tx.amount)}</b></div><div><small>Kỳ hạn</small><b>${term} tháng</b></div>
    <div><small>Lãi suất</small><b>${rate}%/năm</b></div><div><small>Trả hàng tháng (ước tính)</small><b>${vnd(monthly)}</b></div>
    <div><small>Tổng lãi (ước tính)</small><b>${vnd(monthly * term - tx.amount)}</b></div></div>`;
}

// ===== NẠP DỮ LIỆU TỪ DATABASE LÊN GIAO DIỆN (chi tiết ở data.js / schema.sql) =====
// Có backend C++ (QWebChannel) → mỗi lần vào trang, data.js gọi database và đổ dữ liệu vào các "sink" dưới đây.
const replaceAll = (arr, rows) => arr.splice(0, arr.length, ...rows);
const greeting = () => { const h = new Date().getHours(); return h < 11 ? "Chào buổi sáng" : h < 14 ? "Chào buổi trưa" : h < 18 ? "Chào buổi chiều" : "Chào buổi tối"; };
const todayText = () => new Date().toLocaleDateString("vi-VN", { weekday: "long", day: "2-digit", month: "long", year: "numeric" });
const primaryAcc = () => state.accounts.find((a) => a.primary) || state.accounts[0] || { name: "Tài khoản thanh toán", number: "—" };

const loadBar = document.createElement("div");
loadBar.id = "loadbar";
document.body.append(loadBar);

const data = createData({
  sinks: {
    profile: (p) => { state.profile = p; },
    accounts: (list) => { state.accounts = list; const main = list.find((a) => a.primary) || list[0]; state.balance = main?.balance ?? 0; },
    savings: (items) => replaceAll(state.savings, items),
    transactions: (rows) => replaceAll(transactions, rows),
    messages: (rows) => {
      replaceAll(messages, rows);
      rows.forEach((item) => { if (item.id) seenNotificationIds.add(item.id); });
      notificationBaselineReady = true;
      state.selectedMessage = 0; state.msgExpanded = null; state.msgBanner = null;
    },
    loanTerms: (rows) => replaceAll(loanTerms, rows),
    loanApps: (rows) => replaceAll(loanApps, rows.map((app) => {
      const notice = messages.find((item) => item.reference === app.id && item.type === "LOAN");
      if (!notice) return app;
      const title = String(notice.title || "").toLowerCase();
      const type = title.includes("yêu cầu bổ sung") ? "INFO"
        : title.includes("chưa được phê duyệt") ? "REJECTED"
        : title.includes("đã được phê duyệt") ? "APPROVED"
        : title.includes("đã giải ngân") ? "DISBURSED"
        : title.includes("tiếp nhận") ? "SUBMITTED" : "";
      return type ? { ...app, result: { type, note: notice.content || notice.preview || notice.title, by: "Ngân hàng", time: notice.time || "" } } : app;
    })),
    activeLoans: (rows) => replaceAll(activeLoans, rows),
    people: (v) => staff.hydrate("people", v),
    audit: (v) => staff.hydrate("audit", v),
    config: (v) => staff.hydrate("config", v),
    stats: (v) => staff.hydrate("stats", v),
  },
  onStatus: ({ status }) => loadBar.classList.toggle("on", status === "loading"),
  getSessionToken: () => state.sessionToken || globalThis.__OOPBANK_SESSION_TOKEN__ || "",
});

function markRead(m) {
  if (!m.unread) return;
  m.unread = false;
  if (m.id != null && hasBackend()) {
    callBackend("notificationBridge", "markRead", { session_token: state.sessionToken, notification_id: m.id })
      .catch((error) => showToast("Không cập nhật được thông báo: " + (error?.message || String(error))));
  }
}

const pageKey = () => (state.role === "CUSTOMER" ? state.page : `staff:${staff.page}`);

// Nạp dữ liệu cho trang đang mở rồi vẽ lại (không vẽ lại khi người dùng đang nhập liệu hoặc đang mở hộp PIN)
function loadCurrent() {
  if (!state.loggedIn || state.role !== "CUSTOMER") return;
  const key = pageKey();
  data.load(key).then(({ changed, errors }) => {
    if (errors.length) showToast("Không tải được dữ liệu: " + errors.map((e) => e.message || e.query).join("; "));
    const busy = /^(INPUT|TEXTAREA|SELECT)$/.test(document.activeElement?.tagName ?? "") || state.showPinModal;
    if (changed && key === pageKey() && !busy) render();
  });
}
document.addEventListener("visibilitychange", () => { if (!document.hidden) { loadCurrent(); pollNotifications(); } });
const notificationPollClock = setInterval(pollNotifications, 6000);
notificationPollClock?.unref?.();

function logoutSession() {
  const authBridge = getAuthBridge();
  if (state.sessionToken && typeof authBridge?.logout === "function") {
    invokeBridgeMethod(authBridge, "logout", [state.sessionToken]).catch(() => {});
  }
  state.sessionToken = "";
  state.userId = "";
  seenNotificationIds.clear();
  notificationBaselineReady = false;
  globalThis.__OOPBANK_SESSION_TOKEN__ = "";
  data.setSessionToken("");
  state.loggedIn = false;
  state.role = "CUSTOMER";
  state.page = "home";
}

const staff = createStaff({ state, icons, icon, brand, render: () => render(), callBackend,
  uploadCreditDocument,
  hasBackend: () => Boolean(getBridge("userBridge")), onLogout: logoutSession,
  loans: { apps: loanApps, docStatus: DOC_STATUS, notify: notifyCustomer, message: pushMessage,
    terms: loanTerms } });

function render() {
  renderPage();
  renderMsgbox();
}

function renderPage() {
  if (!state.loggedIn) return renderAuth();
  if (state.role !== "CUSTOMER") return staff.render();
  if (state.page === "home") return renderHome();
  if (state.page === "services") return renderServices();
  if (state.page === "accounts") return renderAccounts();
  if (state.page === "credit") return renderCreditPage();
  if (state.page === "notifications") return renderNotifications();
  if (state.page === "transactions") return renderTransactions();
  if (state.page === "user-settings") return renderUserSettings();
  if (state.page === "apply-loan" && state.loanTab !== "apply") return renderLoanHub();
  renderTransactionForm();
}

app.addEventListener("click", async (event) => {
  const staffPageBefore = staff.page;
  if (staff.click(event)) {
    if (staff.page !== staffPageBefore) loadCurrent();
    return;
  }
  const pageButton = event.target.closest("[data-page]");
  const authButton = event.target.closest("[data-auth-mode]");
  const actionButton = event.target.closest("[data-action]");
  const loanPayButton = event.target.closest("[data-loan-pay]");
  const billButtonElement = event.target.closest("[data-bill]");
  const messageButton = event.target.closest("[data-message]");

  const loanTabButton = event.target.closest("[data-loan-tab]");
  const docAppButton = event.target.closest("[data-doc-app]");

  if (loanPayButton) {
    if (!loanPayButton.dataset.loanPay) return showToast("Không còn kỳ vay cần thanh toán.");
    state.pendingTransaction = {
      type: "LOAN_REPAYMENT",
      scheduleId: loanPayButton.dataset.loanPay,
      loanId: loanPayButton.dataset.loanId || "",
      amount: Number(loanPayButton.dataset.loanAmount || 0),
      accountId: loanPayButton.dataset.loanAccount || "",
    };
    state.showPinModal = true;
    render();
  } else if (loanTabButton) {
    state.page = "apply-loan";
    state.loanTab = loanTabButton.dataset.loanTab;
    if (loanTabButton.dataset.docApp) state.docAppId = loanTabButton.dataset.docApp;
    state.transactionSuccess = false;
    window.scrollTo({ top: 0, behavior: "smooth" });
    render();
    loadCurrent();
  } else if (docAppButton) {
    state.docAppId = docAppButton.dataset.docApp;
    render();
  } else if (pageButton) {
    let target = pageButton.dataset.page;
    if (target === "loan") { target = "apply-loan"; state.loanTab = "manage"; }
    else if (target === "apply-loan") state.loanTab = "apply";
    state.page = target;
    state.transactionSuccess = false;
    window.scrollTo({ top: 0, behavior: "smooth" });
    render();
    ensureBillProviders();
    if (target === "credit" && hasBackend()) loadCreditPage();
    loadCurrent();
  } else if (authButton) {
    state.authMode = authButton.dataset.authMode;
    render();
  } else if (billButtonElement) {
    state.billType = billButtonElement.dataset.bill;
    render();
  } else if (messageButton) {
    state.selectedMessage = Number(messageButton.dataset.message);
    render();
  } else if (actionButton?.dataset.action === "toggle-balance") {
    state.showBalance = !state.showBalance;
    render();
  } else if (actionButton?.dataset.action === "toggle-password") {
    const passwordInput = document.getElementById(actionButton.dataset.passwordTarget || "");
    if (!passwordInput) return;
    const mouseHoverReveal = event.detail > 0 && window.matchMedia("(hover: hover) and (pointer: fine)").matches;
    setPasswordVisibility(actionButton, mouseHoverReveal || passwordInput.type === "password");
  } else if (actionButton?.dataset.action === "logout") {
    logoutSession();
    render();
  } else if (actionButton?.dataset.action === "close-modal") {
    state.showPinModal = false;
    state.pendingTransaction = null;
    render();
  } else if (actionButton?.dataset.action === "close-savings-request") {
    const savingsId = actionButton.dataset.savingsId;
    const saving = state.savings.find((item) => item.savings_id === savingsId);
    if (!saving) return showToast("Không tìm thấy sổ tiết kiệm đang hoạt động.");
    state.pendingTransaction = { type: "CLOSE_SAVINGS", savingsId, amount: Number(saving.principal_vnd || 0) };
    state.showPinModal = true;
    render();
  } else if (actionButton?.dataset.action === "toggle-schedule") {
    state.openSchedule = state.openSchedule === actionButton.dataset.id ? null : actionButton.dataset.id;
    render();
  } else if (actionButton?.dataset.action === "forgot-pass") {
    state.authMode = "forgot";
    render();
  }
});

app.addEventListener("pointerover", (event) => {
  if (event.pointerType !== "mouse") return;
  const button = event.target.closest?.(".password-toggle");
  if (button && !button.contains(event.relatedTarget)) setPasswordVisibility(button, true);
});

app.addEventListener("pointerout", (event) => {
  if (event.pointerType !== "mouse") return;
  const button = event.target.closest?.(".password-toggle");
  if (button && !button.contains(event.relatedTarget)) setPasswordVisibility(button, false);
});

app.addEventListener("change", async (event) => {
  const input = event.target;
  if (input.matches?.("input[type=file][data-credit-application]") && input.files?.[0]) {
    const file = input.files[0];
    if (file.size > 5 * 1024 * 1024) return showToast("Tệp vượt quá 5 MB, vui lòng chọn tệp nhỏ hơn.");
    try {
      const storageKey = await uploadCreditDocument(file);
      await callBackend("creditBridge", "submitDocument", {
        session_token: state.sessionToken, application_id: input.dataset.creditApplication,
        file_name: file.name, mime_type: file.type, file_size: file.size, storage_key: storageKey,
      });
      showToast("Đã gửi lại giấy tờ để Admin kiểm tra.");
      await loadCreditPage();
    } catch (error) { showToast("Không gửi được giấy tờ: " + (error?.message || String(error))); }
    return;
  }
  if (!input.matches?.("input[type=file][data-doc]") || !input.files[0]) return;
  const file = input.files[0];
  if (file.size <= 0 || file.size > 5 * 1024 * 1024) return showToast("Tệp phải có dung lượng từ 1 byte đến 5 MB.");
  const doc = docApp()?.docs.find((d) => d.id === input.dataset.doc);
  if (!doc) return;
  if (!hasBackend()) return showToast("Cần kết nối backend để tải giấy tờ vay lên.");
  try {
      if (!["application/pdf", "image/jpeg", "image/png"].includes(file.type)) {
        return showToast("Chỉ nhận tệp PDF, JPG hoặc PNG.");
      }
      const payload = new FormData();
      payload.append("file", file);
      payload.append("upload_preset", CLOUDINARY_UPLOAD_PRESET);
      const uploadResponse = await fetch(`https://api.cloudinary.com/v1_1/${CLOUDINARY_CLOUD_NAME}/auto/upload`, {
        method: "POST", body: payload,
      });
      const uploaded = await uploadResponse.json();
      if (!uploadResponse.ok || !uploaded.secure_url) {
        throw new Error(uploaded.error?.message || "Cloudinary không nhận được tệp.");
      }
      await callBackend("loanBridge", "submitDocument", {
        session_token: state.sessionToken, loan_id: input.dataset.loan,
        document_type: input.dataset.doc, file_name: file.name, mime_type: file.type,
        file_size: file.size, storage_key: uploaded.secure_url,
      });
      showToast("Đã gửi tài liệu để kiểm tra.");
      loadCurrent();
  } catch (error) {
    showToast("Không tải được tài liệu: " + (error?.message || String(error)));
  }
});

app.addEventListener("submit", async (event) => {
  event.preventDefault();
  if (await staff.submit(event)) return;

  if (event.target.id === "credit-apply-form") {
    const form = new FormData(event.target);
    const limit = Number(form.get("limit") || 0);
    const income = Number(form.get("income") || 0);
    const file = form.get("income_document");
    const maxLimit = Math.min(500000000, income * 6);
    if (!Number.isSafeInteger(income) || income < 1000000 || income > 10000000000) return showToast("Thu nhập tháng phải từ 1.000.000 đến 10.000.000.000 đồng.");
    if (!Number.isSafeInteger(limit) || limit < 1000000 || limit > maxLimit) return showToast("Hạn mức đề nghị không được vượt quá 6 lần thu nhập tháng hoặc 500.000.000 đồng.");
    if (!(file instanceof File) || !file.size) return showToast("Chọn giấy tờ chứng minh thu nhập trước khi gửi hồ sơ.");
    try {
      const storageKey = await uploadCreditDocument(file);
      const result = await callBackend("creditBridge", "apply", {
        session_token: state.sessionToken, requested_limit_vnd: limit, monthly_income_vnd: income,
        file_name: file.name, mime_type: file.type, file_size: file.size, storage_key: storageKey,
      });
      state.creditApplications = result.applications || state.creditApplications;
      showToast("Đã gửi hồ sơ xin hạn mức tới Admin."); render();
    } catch (error) { showToast("Không gửi được hồ sơ: " + (error?.message || String(error))); }
    return;
  }
  if (event.target.id === "credit-spend-form" || event.target.id === "credit-repay-form") {
    const isSpend = event.target.id === "credit-spend-form";
    const creditForm = event.target;
    const formData = new FormData(event.target);
    const amount = Number(formData.get("amount") || 0);
    const pin = String(formData.get("pin") || "");
    if (!Number.isSafeInteger(amount) || amount <= 0) return showToast("Nhập số tiền hợp lệ.");
    if (!/^\d{6}$/.test(pin)) return showToast("Mã PIN giao dịch phải có 6 chữ số.");
    const receiver = String(formData.get("credit-receiver") || "").trim();
    if (isSpend && (!/^\d{8,16}$/.test(receiver))) return showToast("Số tài khoản nhận phải gồm 8–16 chữ số.");
    const fingerprint = JSON.stringify([isSpend ? "spend" : "repay", state.creditAccount?.account_id || "",
      String(formData.get("source_account_id") || ""), receiver, amount]);
    if (creditForm.dataset.requestFingerprint && creditForm.dataset.requestFingerprint !== fingerprint) {
      return showToast("Yêu cầu tín dụng trước chưa rõ kết quả. Hãy giữ nguyên tài khoản, người nhận và số tiền để gửi lại.");
    }
    creditForm.dataset.requestFingerprint = fingerprint;
    if (!creditForm.dataset.idempotencyKey) creditForm.dataset.idempotencyKey = numericIdempotencyKey("CRE");
    try {
      const action = isSpend ? "spend" : "repay";
      const payload = { session_token: state.sessionToken, idempotency_key: creditForm.dataset.idempotencyKey, account_id: state.creditAccount?.account_id || "", receiver_account_id: "", receiver_bank: "OOPBANK", source_account_id: String(formData.get("source_account_id") || ""), amount_vnd: amount, pin, description: isSpend ? "Chi tiêu hạn mức tín dụng" : "Trả nợ hạn mức tín dụng" };
      if (isSpend) { payload.receiver_account_id = receiver; payload.receiver_bank = String(formData.get("bank") || "OOPBANK"); }
      const result = await callBackend("creditBridge", action, payload);
      delete creditForm.dataset.idempotencyKey;
      delete creditForm.dataset.requestFingerprint;
      showToast(result.message || "Giao dịch tín dụng đã hoàn tất.");
      await loadCreditPage();
    } catch (error) { showToast("Giao dịch tín dụng thất bại: " + (error?.message || String(error))); }
    return;
  }

  if (event.target.id === "forgot-password-form") {
    const authBridge = getAuthBridge();
    if (typeof authBridge?.forgotPassword !== "function") {
      showToast("Chức năng đặt lại mật khẩu cần kết nối database trong ứng dụng desktop.");
      return;
    }

    const formData = new FormData(event.target);
    const identifier = String(formData.get("forgot-identifier") || "").trim();
    const cccd = String(formData.get("forgot-cccd") || "").trim();
    const pin = String(formData.get("forgot-pin") || "").trim();
    const new_password = String(formData.get("forgot-new-password") || "");
    const confirmPassword = String(formData.get("forgot-confirm-password") || "");
    if (!/^\d{12}$/.test(cccd)) {
      showToast("Số CCCD phải gồm đúng 12 chữ số.");
      return;
    }
    if (!/^\d{6}$/.test(pin)) {
      showToast("Mã PIN phải gồm đúng 6 chữ số.");
      return;
    }
    if (new_password.length < 8) {
      showToast("Mật khẩu mới cần có ít nhất 8 ký tự.");
      return;
    }
    if (new_password !== confirmPassword) {
      showToast("Mật khẩu xác nhận chưa khớp.");
      return;
    }

    try {
      const result = await callBackend("authBridge", "forgotPassword", { identifier, cccd, pin, new_password });
      state.authMode = "login";
      render();
      showToast(result.message || "Đặt lại mật khẩu thành công. Vui lòng đăng nhập lại.");
    } catch (error) {
      showToast("Không đặt lại được mật khẩu: " + (error?.message || String(error)));
    }
    return;
  }

  // 1. Đăng nhập / Đăng ký
  if (event.target.id === "auth-form") {
    if (state.authMode === "login") {
      const identifier = document.querySelector("#contact")?.value ?? "";
      const password = document.querySelector("#password")?.value ?? "";
      const authBridge = getAuthBridge();

      if (typeof authBridge?.login === "function") {
        try {
          const response = parseBridgeResponse(await invokeBridgeMethod(
            authBridge,
            "login",
            [JSON.stringify({ identifier, password })],
          ));
          if (!response?.success) {
            showToast(response?.message || "Đăng nhập thất bại.");
            return;
          }

          const session = response.session ?? {};
          if (!session.session_token) {
            showToast("Backend đăng nhập chưa trả session token.");
            return;
          }

          state.sessionToken = session.session_token;
          state.userId = session.user_id || "";
          data.setSessionToken(state.sessionToken);
          state.loggedIn = true;
          state.role = String(session.role || "CUSTOMER").toUpperCase();
          if (state.role === "CUSTOMER") {
            state.page = "home";
            state.accounts = [];
            state.balance = 0;
            state.profile = {
              name: session.full_name || "",
              email: "",
              phone: "",
              branch: "OOPBank",
            };
            transactions.splice(0);
            messages.splice(0);
            seenNotificationIds.clear();
            notificationBaselineReady = false;
            state.savings.splice(0);
            loanApps.splice(0);
            activeLoans.splice(0);
            loanTerms.splice(0);
          }
          else staff.login(state.role);
        } catch (error) {
          showToast("Không thể đăng nhập: " + (error?.message || String(error)));
          return;
        }
      } else {
        showToast("Không tìm thấy dịch vụ xác thực. Hãy mở ứng dụng qua desktop host.");
        return;
      }
    } else {
      if (typeof getAuthBridge()?.registerCustomer !== "function") {
        showToast("Không thể đăng ký khi chưa kết nối database. Vui lòng cấu hình TURSO_URL và TURSO_TOKEN trong .env.");
        return;
      }
      try {
        const formData = new FormData(event.target);
        const result = await callBackend("authBridge", "registerCustomer", {
          username: String(formData.get("username") || "").trim(),
          password: String(formData.get("password") || ""),
          fullname: String(formData.get("name") || "").trim(),
          email: String(formData.get("contact") || "").trim(),
          cccd: String(formData.get("cccd") || "").trim(),
          phone: String(formData.get("phone") || "").trim(),
        });
        state.authMode = "login";
        render();
        showToast(result.message || "Đăng ký tài khoản thành công. Vui lòng đăng nhập.");
      } catch (error) {
        showToast("Không thể đăng ký: " + (error?.message || String(error)));
        return;
      }
    }
    render();
    loadCurrent();
  }

  // 2. Submit form giao dịch -> Mở Modal xác thực PIN & OTP
  if (event.target.id === "transaction-form") {
    const formData = new FormData(event.target);
    const values = Object.fromEntries(formData.entries());
    const amountVal = Number(formData.get("amount") || formData.get("saving-amount") || formData.get("loan-amount") || 0);
    if (state.page === "loan-pay") return showToast("Hãy chọn kỳ vay cần trả trong mục Quản lý tín dụng.");
    if (state.page === "transfer" && !/^\d{8,16}$/.test(String(values.receiver || "").trim())) return showToast("Số tài khoản thụ hưởng phải gồm 8–16 chữ số.");
    if (["transfer", "savings"].includes(state.page) && amountVal > state.balance) return showToast("Số dư khả dụng không đủ để thực hiện giao dịch");
    if (state.page === "apply-loan") {
      const term = Number(values.loan_term || 0);
      const monthlyIncome = Number(values["monthly-income"] || 0);
      const purpose = String(values["loan-purpose"] || "").trim();
      if (!Number.isSafeInteger(amountVal) || amountVal <= 0) return showToast("Số tiền vay phải là số nguyên dương.");
      if (!Number.isSafeInteger(monthlyIncome) || monthlyIncome <= 0 || monthlyIncome > 1000000000000) return showToast("Thu nhập hàng tháng phải là số nguyên dương và không vượt quá 1.000.000.000.000 đồng.");
      if (!purpose || purpose.length > 300) return showToast("Hãy nhập mục đích vay, tối đa 300 ký tự.");
      if (!loanTerms.some((item) => item.months === term && item.enabled)) return showToast("Kỳ hạn vay này hiện không được mở.");
      if (!["DECREASING", "FIXED"].includes(values.calc_method)) return showToast("Phương thức trả nợ không hợp lệ.");
      try {
        const result = await callBackend("loanBridge", "applyLoan", {
          session_token: state.sessionToken,
          amount_vnd: amountVal,
          term_months: Number(values.loan_term || 12),
          calc_method: values.calc_method === "FIXED" ? "FLAT" : (values.calc_method || "DECREASING"),
          collateral_info: String(values.collateral || "").trim(),
          purpose: String(values["loan-purpose"] || "").trim(),
          monthly_income_vnd: Number(values["monthly-income"] || 0),
        });
        state.docAppId = result.loan_id || null;
        state.transactionSuccess = true;
        state.transactionSuccessMessage = result.message || "Hồ sơ đã được lưu. Tiếp tục nộp giấy tờ để ngân hàng thẩm định.";
        render();
        loadCurrent();
      } catch (error) { showToast("Không thể nộp đơn vay: " + (error?.message || String(error))); }
      return;
    }
    if (state.page === "payment") {
      try {
        const lookup = await callBackend("billBridge", "queryBill", {
          session_token: state.sessionToken,
          provider_id: String(formData.get("provider") || "").trim(),
          customer_code: String(formData.get("bill-code") || "").trim(),
        });
        const bill = lookup.bill || {};
        state.pendingTransaction = { type: "payment", amount: Number(bill.amount_vnd || 0), data: { ...values, bill_id: bill.bill_id }, idempotencyKey: numericIdempotencyKey("BILL") };
        state.showPinModal = true;
        render();
      } catch (error) { showToast("Không tra cứu được hóa đơn: " + (error?.message || String(error))); }
      return;
    }
    if (["transfer", "savings"].includes(state.page) && amountVal > state.balance) return showToast("Số dư khả dụng không đủ để thực hiện giao dịch");
    state.pendingTransaction = {
      type: state.page,
      amount: amountVal,
      data: values,
      idempotencyKey: numericIdempotencyKey("TXN"),
    };
    state.showPinModal = true;
    render();
    if (state.page === "transfer") maybeSendOtp();
  }

  // 3. Submit Modal PIN/OTP -> Xác nhận hoàn tất
  if (event.target.id === "pin-modal-form") {
    const tx = state.pendingTransaction;
    const formData = new FormData(event.target);
    const pin = String(formData.get("pin") || "");
    const otp = String(formData.get("otp") || "");
    state.showPinModal = false;
    try {
      if (!tx) throw new Error("Không có giao dịch chờ xác thực.");
      const common = { session_token: state.sessionToken, pin };
      let loanRepaymentResult = null;
      let operationResult = null;
      if (tx.type === "transfer") {
        operationResult = await callBackend("transactionBridge", "transfer", {
          ...common,
          idempotency_key: tx.idempotencyKey,
          receiver_account_id: String(tx.data.receiver || "").trim(),
          receiver_bank: String(tx.data.bank || "OOPBANK"),
          amount_vnd: tx.amount,
          description: "Chuyển tiền",
          otp,
          otp_challenge_id: tx.otpChallengeId || "",
          is_interbank: String(tx.data.bank || "OOPBANK") !== "OOPBANK",
        });
      } else if (tx.type === "topup") {
        operationResult = await callBackend("transactionBridge", "topup", {
          ...common, idempotency_key: tx.idempotencyKey, carrier: String(tx.data.carrier || ""),
          phone: String(tx.data["topup-phone"] || "").replace(/\D/g, ""), amount_vnd: tx.amount,
        });
      } else if (tx.type === "payment") {
        operationResult = await callBackend("billBridge", "payBill", {
          ...common,
          idempotency_key: tx.idempotencyKey,
          bill_id: tx.data.bill_id,
        });
      } else if (tx.type === "savings") {
        operationResult = await callBackend("savingsBridge", "openSavings", {
          ...common,
          idempotency_key: tx.idempotencyKey,
          amount_vnd: tx.amount,
          term_months: Number(tx.data.term || 6),
          maturity_option: String(tx.data.maturity_option || "CLOSE_TO_ACCOUNT"),
        });
      } else if (tx.type === "CLOSE_SAVINGS") {
        operationResult = await callBackend("savingsBridge", "closeSavings", { ...common, savings_id: tx.savingsId });
      } else if (tx.type === "LOAN_REPAYMENT") {
        loanRepaymentResult = operationResult = await callBackend("loanBridge", "payInstallment", {
          ...common, schedule_id: tx.scheduleId, account_id: tx.accountId,
        });
      } else {
        throw new Error("Loại giao dịch này chưa có backend.");
      }
      state.pendingTransaction = null;
      state.transactionSuccess = tx.type !== "LOAN_REPAYMENT";
      state.transactionSuccessMessage = operationResult?.message || "Yêu cầu của bạn đã được ghi nhận.";
      render();
      if (loanRepaymentResult) showToast(`Đã thanh toán kỳ vay ${vnd(Number(loanRepaymentResult.amount_vnd || loanRepaymentResult.total_due_vnd || tx.amount))}.`);
      loadCurrent();
    } catch (error) {
      state.pendingTransaction = tx;
      state.showPinModal = Boolean(tx);
      render();
      showToast("Chưa xác nhận được kết quả. " + (error?.message || "") + " Nếu thử lại, mã chống gửi trùng sẽ được giữ nguyên.");
    }
  }

  // 4. Submit các Form cập nhật hồ sơ & đổi pass/PIN
  if (event.target.id === "update-profile-form") {
    const formData = new FormData(event.target);
    try {
      await callBackend("userBridge", "updateProfile", {
        session_token: state.sessionToken,
        fullname: String(formData.get("fullname") || "").trim(),
        email: String(formData.get("email") || "").trim(),
        phone: String(formData.get("phone") || "").trim(),
      });
      showToast("Đã cập nhật hồ sơ cá nhân.");
      loadCurrent();
    } catch (error) { showToast("Không thể cập nhật hồ sơ: " + (error?.message || String(error))); }
  }
  if (event.target.id === "change-pass-form") {
    const formData = new FormData(event.target);
    try {
      await callBackend("userBridge", "changePassword", { session_token: state.sessionToken, old_password: formData.get("old_password"), new_password: formData.get("new_password") });
      showToast("Đã cập nhật mật khẩu mới.");
      event.target.reset();
    } catch (error) { showToast("Không thể đổi mật khẩu: " + (error?.message || String(error))); }
  }
  if (event.target.id === "change-pin-form") {
    const formData = new FormData(event.target);
    try {
      await callBackend("userBridge", "changePin", { session_token: state.sessionToken, old_pin: formData.get("old_pin"), new_pin: formData.get("new_pin") });
      showToast("Đã cập nhật mã PIN giao dịch.");
      event.target.reset();
    } catch (error) { showToast("Không thể đổi mã PIN: " + (error?.message || String(error))); }
  }
});

render();
loadCurrent();
