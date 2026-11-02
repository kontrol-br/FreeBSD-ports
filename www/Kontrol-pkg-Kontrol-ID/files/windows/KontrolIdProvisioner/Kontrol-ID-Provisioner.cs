using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.NetworkInformation;
using System.Net.Security;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Security.Cryptography.X509Certificates;
using System.Security.Principal;
using System.Text;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows.Forms;

[assembly: AssemblyTitle("Kontrol-ID Provisioner")]
[assembly: AssemblyDescription("Provisionamento seguro de estações Windows para o Kontrol-ID")]
[assembly: AssemblyCompany("Kontrol")]
[assembly: AssemblyProduct("Kontrol-ID Provisioner")]
[assembly: AssemblyCopyright("Copyright © 2016-2026 Kontrol Tecnologia EPP")]
[assembly: AssemblyVersion("0.5.0.51")]
[assembly: AssemblyFileVersion("0.5.0.51")]
[assembly: AssemblyInformationalVersion("0.5.0_51")]

namespace Kontrol.Identity.Provisioner
{
    internal static class Program
    {
        [STAThread]
        private static void Main(string[] args)
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new ProvisionerForm());
        }
    }

    internal sealed class ProvisionerForm : Form
    {
        private readonly TextBox appliance = new TextBox();
        private readonly TextBox username = new TextBox();
        private readonly TextBox password = new TextBox();
        private readonly TextBox log = new TextBox();
        private readonly Button install = new Button();
        private readonly Button remove = new Button();
        private readonly Label packageStatus = new Label();
        private readonly Label stationStatus = new Label();
        private bool busy;
        private bool stationConfigured;
        private readonly EmbeddedConfiguration packageConfiguration;

        public ProvisionerForm()
        {
            packageConfiguration = LoadEmbeddedConfiguration();
            Text = "Kontrol-ID Provisioner";
            using (System.Drawing.Icon appIcon = System.Drawing.Icon.ExtractAssociatedIcon(Application.ExecutablePath)) {
                if (appIcon != null)
                    Icon = (System.Drawing.Icon)appIcon.Clone();
            }
            StartPosition = FormStartPosition.CenterScreen;
            AutoScaleMode = AutoScaleMode.Dpi;
            ClientSize = new Size(450, 398);
            FormBorderStyle = FormBorderStyle.FixedSingle;
            MaximizeBox = false;
            Font = new Font("Segoe UI", 9F);
            BackColor = Color.FromArgb(245, 247, 249);

            var header = new Panel {
                Location = new Point(0, 0),
                Size = new Size(450, 60),
                BackColor = Color.FromArgb(49, 47, 48),
                Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right
            };
            var logo = new PictureBox {
                Location = new Point(14, 12),
                Size = new Size(120, 35),
                SizeMode = PictureBoxSizeMode.Zoom,
                BackColor = Color.Transparent,
                Image = LoadLogo()
            };
            var product = new Label {
                Text = "KONTROL-ID PROVISIONER",
                Font = new Font("Segoe UI Semibold", 10F),
                ForeColor = Color.White,
                AutoSize = true,
                Location = new Point(150, 11)
            };
            var subtitle = new Label {
                Text = "Configuração segura da estação Windows",
                ForeColor = Color.FromArgb(205, 213, 220),
                AutoSize = true,
                Location = new Point(151, 33)
            };
            header.Controls.Add(logo);
            header.Controls.Add(product);
            header.Controls.Add(subtitle);
            Controls.Add(header);

            var applianceLabel = new Label {
                Text = "Endereço HTTPS do Kontrol",
                AutoSize = true,
                Location = new Point(20, 68)
            };
            appliance.Location = new Point(20, 87);
            appliance.Size = new Size(410, 23);
            appliance.BackColor = Color.White;
            appliance.Text = packageConfiguration == null ? "https://" : packageConfiguration.ApplianceUrl;
            appliance.TextChanged += (s, e) => UpdateActionState();
            packageStatus.Location = new Point(20, 115);
            packageStatus.Size = new Size(410, 34);
            packageStatus.ForeColor = Color.FromArgb(110, 83, 0);
            packageStatus.Text = packageConfiguration == null
                ? "Configuração de confiança ausente. Baixe este Provisioner na GUI do Kontrol-ID."
                : "CA pública incorporada. A autorização será concluída diretamente pela API segura.";
            Controls.Add(applianceLabel);
            Controls.Add(appliance);
            Controls.Add(packageStatus);

            var usernameLabel = new Label {
                Text = "Administrador do Kontrol",
                AutoSize = true,
                Location = new Point(20, 151)
            };
            username.Location = new Point(20, 170);
            username.Size = new Size(198, 23);
            username.TextChanged += (s, e) => UpdateActionState();
            var passwordLabel = new Label {
                Text = "Senha",
                AutoSize = true,
                Location = new Point(232, 151)
            };
            password.Location = new Point(232, 170);
            password.Size = new Size(198, 23);
            password.UseSystemPasswordChar = true;
            password.TextChanged += (s, e) => UpdateActionState();
            Controls.Add(usernameLabel);
            Controls.Add(username);
            Controls.Add(passwordLabel);
            Controls.Add(password);

            install.Text = "Autorizar e configurar";
            install.Location = new Point(20, 205);
            install.Size = new Size(198, 34);
            install.BackColor = Color.FromArgb(23, 105, 170);
            install.ForeColor = Color.White;
            install.FlatStyle = FlatStyle.Flat;
            install.FlatAppearance.BorderSize = 0;
            install.Click += async (s, e) => await InstallAsync();
            Controls.Add(install);

            remove.Text = "Remover configuração";
            remove.Location = new Point(232, 205);
            remove.Size = new Size(198, 34);
            remove.FlatStyle = FlatStyle.Flat;
            remove.Click += async (s, e) => await RemoveAsync();
            Controls.Add(remove);

            log.Location = new Point(20, 249);
            log.Size = new Size(410, 74);
            log.Multiline = true;
            log.ReadOnly = true;
            log.ScrollBars = ScrollBars.Vertical;
            log.BackColor = Color.White;
            log.Font = new Font("Consolas", 8.25F);
            Controls.Add(log);

            stationStatus.Location = new Point(20, 329);
            stationStatus.Size = new Size(410, 19);
            stationStatus.Font = new Font("Segoe UI Semibold", 8.5F);
            Controls.Add(stationStatus);

            var footer = new Label {
                Text = "2016–" + DateTime.Now.Year + " - Copyright - Kontrol Tecnologia EPP - Powered by KONTROL.",
                ForeColor = Color.FromArgb(105, 115, 124),
                Font = new Font("Segoe UI", 6.75F),
                TextAlign = ContentAlignment.MiddleCenter,
                Location = new Point(10, 370),
                Size = new Size(430, 18)
            };
            Controls.Add(footer);

            RefreshStationState();

            Shown += (s, e) => {
                Append("Pronto. Executando com privilégios administrativos.");
            };
        }

        private static Image LoadLogo()
        {
            using (Stream stream = Assembly.GetExecutingAssembly().GetManifestResourceStream("KontrolIdProvisioner.Logo")) {
                if (stream == null) return null;
                using (Image source = Image.FromStream(stream)) return new Bitmap(source);
            }
        }

        private sealed class EmbeddedConfiguration
        {
            internal string ApplianceUrl;
            internal string CaPem;
        }

        private static EmbeddedConfiguration LoadEmbeddedConfiguration()
        {
            const int footerLength = 44;
            byte[] magic = Encoding.ASCII.GetBytes("KIDCFG01");
            try {
                using (var stream = new FileStream(Application.ExecutablePath, FileMode.Open,
                    FileAccess.Read, FileShare.Read)) {
                    if (stream.Length < footerLength) return null;
                    stream.Seek(-footerLength, SeekOrigin.End);
                    byte[] expectedHash = new byte[32];
                    if (stream.Read(expectedHash, 0, expectedHash.Length) != expectedHash.Length) return null;
                    byte[] lengthBytes = new byte[4];
                    if (stream.Read(lengthBytes, 0, lengthBytes.Length) != lengthBytes.Length) return null;
                    byte[] observedMagic = new byte[magic.Length];
                    if (stream.Read(observedMagic, 0, observedMagic.Length) != observedMagic.Length ||
                        !ConstantTimeEquals(magic, observedMagic)) return null;
                    int payloadLength = ReadNetworkInt32(lengthBytes, 0);
                    if (payloadLength < 16 || payloadLength > 65536 ||
                        payloadLength > stream.Length - footerLength) return null;
                    byte[] payload = new byte[payloadLength];
                    stream.Seek(-(footerLength + payloadLength), SeekOrigin.End);
                    if (stream.Read(payload, 0, payload.Length) != payload.Length) return null;
                    using (SHA256 sha = SHA256.Create()) {
                        byte[] observedHash = sha.ComputeHash(payload);
                        if (!ConstantTimeEquals(expectedHash, observedHash)) return null;
                    }
                    int offset = 0;
                    if (payload.Length < 12 || Encoding.ASCII.GetString(payload, 0, 4) != "KID1") return null;
                    offset += 4;
                    int urlLength = ReadNetworkInt32(payload, offset);
                    offset += 4;
                    if (urlLength < 8 || urlLength > 2048 || offset + urlLength + 4 > payload.Length) return null;
                    string applianceUrl = Encoding.UTF8.GetString(payload, offset, urlLength);
                    offset += urlLength;
                    int caLength = ReadNetworkInt32(payload, offset);
                    offset += 4;
                    if (caLength < 128 || caLength > 61440 || offset + caLength != payload.Length) return null;
                    string caPem = Encoding.ASCII.GetString(payload, offset, caLength);
                    Uri uri;
                    if (!Uri.TryCreate(applianceUrl, UriKind.Absolute, out uri) ||
                        !String.Equals(uri.Scheme, Uri.UriSchemeHttps, StringComparison.OrdinalIgnoreCase)) return null;
                    var ca = new X509Certificate2(Encoding.ASCII.GetBytes(caPem));
                    var constraints = ca.Extensions.OfType<X509BasicConstraintsExtension>().FirstOrDefault();
                    if (constraints == null || !constraints.CertificateAuthority ||
                        DateTime.Now < ca.NotBefore || DateTime.Now > ca.NotAfter) return null;
                    return new EmbeddedConfiguration {
                        ApplianceUrl = uri.GetLeftPart(UriPartial.Authority), CaPem = caPem
                    };
                }
            } catch {
                return null;
            }
        }

        private static int ReadNetworkInt32(byte[] value, int offset)
        {
            if (value == null || offset < 0 || offset + 4 > value.Length) return -1;
            uint result = ((uint)value[offset] << 24) | ((uint)value[offset + 1] << 16) |
                ((uint)value[offset + 2] << 8) | value[offset + 3];
            return result > Int32.MaxValue ? -1 : (int)result;
        }

        private static bool ConstantTimeEquals(byte[] left, byte[] right)
        {
            if (left == null || right == null) return false;
            int difference = left.Length ^ right.Length;
            int length = Math.Max(left.Length, right.Length);
            for (int index = 0; index < length; index++) {
                byte a = index < left.Length ? left[index] : (byte)0;
                byte b = index < right.Length ? right[index] : (byte)0;
                difference |= a ^ b;
            }
            return difference == 0;
        }

        private void RefreshStationState()
        {
            using (RegistryKey key = Registry.LocalMachine.OpenSubKey(@"SOFTWARE\Kontrol\Kontrol-ID")) {
                stationConfigured = key != null;
                string configuredAppliance = key == null ? "" : Convert.ToString(key.GetValue("Appliance", ""));
                if (!String.IsNullOrWhiteSpace(configuredAppliance))
                    appliance.Text = configuredAppliance;
                stationStatus.Text = stationConfigured
                    ? "● Estação configurada" + (String.IsNullOrWhiteSpace(configuredAppliance) ? "" : " para " + configuredAppliance)
                    : "○ Estação ainda não configurada";
                stationStatus.ForeColor = stationConfigured ? Color.FromArgb(28, 113, 62) : Color.FromArgb(105, 115, 124);
            }
            UpdateActionState();
        }

        private bool ApplianceReady()
        {
            Uri uri;
            return Uri.TryCreate(appliance.Text.Trim(), UriKind.Absolute, out uri) &&
                String.Equals(uri.Scheme, Uri.UriSchemeHttps, StringComparison.OrdinalIgnoreCase) &&
                !String.IsNullOrWhiteSpace(uri.Host);
        }

        private void UpdateActionState()
        {
            install.Enabled = !busy && packageConfiguration != null && ApplianceReady() &&
                !String.IsNullOrWhiteSpace(username.Text) && password.TextLength > 0;
            remove.Enabled = !busy && stationConfigured;
            UseWaitCursor = busy;
        }

        private static long UnixNow()
        {
            return (long)(DateTime.UtcNow - new DateTime(1970, 1, 1, 0, 0, 0, DateTimeKind.Utc)).TotalSeconds;
        }

        private async Task InstallAsync()
        {
            Uri baseUri;
            if (!Uri.TryCreate(appliance.Text.Trim(), UriKind.Absolute, out baseUri) ||
                !String.Equals(baseUri.Scheme, Uri.UriSchemeHttps, StringComparison.OrdinalIgnoreCase)) {
                MessageBox.Show("Informe um endereço HTTPS válido para o Kontrol.", Text,
                    MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            SetBusy(true);
            log.Clear();
            string administrator = username.Text.Trim();
            string administratorPassword = password.Text;
            password.Clear();
            try {
                if (!NativeMethods.IsAdministrator())
                    throw new InvalidOperationException("Execute o Provisioner com privilégios administrativos.");
                if (packageConfiguration == null)
                    throw new InvalidOperationException("Baixe um Provisioner configurado diretamente na GUI do Kontrol-ID.");
                var trustCa = new X509Certificate2(Encoding.ASCII.GetBytes(packageConfiguration.CaPem));
                AddCertificate(StoreName.Root, trustCa);
                ApiAuthorization authorization = await Task.Run(() =>
                    AuthorizeByApi(baseUri, administrator, administratorPassword, trustCa.Thumbprint));
                administratorPassword = null;
                await Task.Run(() => Provision(baseUri, authorization.Code, authorization.Verifier,
                    trustCa.Thumbprint, authorization.ServerFingerprint));
                RefreshStationState();
                packageStatus.Text = "Autorização consumida com segurança. Nenhum manifesto ou credencial foi salvo no disco.";
                packageStatus.ForeColor = Color.FromArgb(28, 113, 62);
                Append("Concluído. A estação está pronta para identificação automática.");
                MessageBox.Show("A configuração do Kontrol-ID foi concluída.", Text,
                    MessageBoxButtons.OK, MessageBoxIcon.Information);
            } catch (Exception ex) {
                Append("ERRO: " + ex.Message);
                MessageBox.Show(ex.Message, "Falha na configuração", MessageBoxButtons.OK,
                    MessageBoxIcon.Error);
            } finally {
                administratorPassword = null;
                SetBusy(false);
            }
        }

        private async Task RemoveAsync()
        {
            if (MessageBox.Show("Remover a configuração do Kontrol-ID desta estação?",
                Text, MessageBoxButtons.YesNo, MessageBoxIcon.Question) != DialogResult.Yes) {
                return;
            }
            SetBusy(true);
            log.Clear();
            try {
                if (!NativeMethods.IsAdministrator())
                    throw new InvalidOperationException("Execute o Provisioner com privilégios administrativos.");
                await Task.Run(() => RemoveProvisioning());
                RefreshStationState();
                Append("CA, certificados, políticas, firewall e conta técnica removidos.");
                MessageBox.Show("A configuração do Kontrol-ID foi removida.", Text,
                    MessageBoxButtons.OK, MessageBoxIcon.Information);
            } catch (Exception ex) {
                Append("ERRO: " + ex.Message);
                MessageBox.Show(ex.Message, "Falha na remoção", MessageBoxButtons.OK,
                    MessageBoxIcon.Error);
            } finally {
                SetBusy(false);
            }
        }

        private void SetBusy(bool busy)
        {
            this.busy = busy;
            UpdateActionState();
        }

        private sealed class ApiAuthorization
        {
            internal string Code;
            internal string Verifier;
            internal string ServerFingerprint;
        }

        private ApiAuthorization AuthorizeByApi(Uri baseUri, string administrator,
            string administratorPassword, string trustCaThumbprint)
        {
            string hostname = Environment.MachineName.ToLowerInvariant();
            string fqdn = GetFqdn().ToLowerInvariant();
            string verifier = RandomBase64Url(32);
            string challenge;
            using (SHA256 sha = SHA256.Create())
                challenge = Base64Url(sha.ComputeHash(Encoding.ASCII.GetBytes(verifier)));
            Append("Autenticando o administrador pela API protegida pela CA incorporada...");
            var serializer = new JavaScriptSerializer { MaxJsonLength = 32768 };
            string requestBody = serializer.Serialize(new Dictionary<string, object> {
                { "username", administrator }, { "password", administratorPassword },
                { "hostname", hostname }, { "fqdn", fqdn }, { "codeChallenge", challenge }
            });
            Uri endpoint = new Uri(baseUri.ToString().TrimEnd('/') + "/kontrol_id_api_authorize.php");
            string responseBody = PostTrustedJson(endpoint, requestBody, trustCaThumbprint);
            requestBody = null;
            var response = serializer.Deserialize<Dictionary<string, object>>(responseBody);
            if (!response.ContainsKey("ok") || !(response["ok"] is bool) || !(bool)response["ok"])
                throw new InvalidOperationException("Usuário ou senha inválidos.");
            string code = GetRequiredString(response, "authorizationCode");
            string fingerprint = NormalizeThumbprint(GetRequiredString(response, "serverCertificateSha256"));
            if (!System.Text.RegularExpressions.Regex.IsMatch(code, "^[A-Za-z0-9_-]{43}$") ||
                fingerprint.Length != 64 || fingerprint.Any(c => !Uri.IsHexDigit(c)))
                throw new InvalidOperationException("O Kontrol retornou uma autorização inválida.");
            Append("Autorização administrativa recebida. Gerando a matrícula...");
            return new ApiAuthorization { Code = code, Verifier = verifier, ServerFingerprint = fingerprint };
        }

        private static string RandomBase64Url(int length)
        {
            byte[] bytes = new byte[length];
            try {
                using (RandomNumberGenerator random = RandomNumberGenerator.Create()) random.GetBytes(bytes);
                return Base64Url(bytes);
            } finally {
                Array.Clear(bytes, 0, bytes.Length);
            }
        }

        private static string Base64Url(byte[] bytes)
        {
            return Convert.ToBase64String(bytes).TrimEnd('=').Replace('+', '-').Replace('/', '_');
        }

        private void Append(string message)
        {
            if (InvokeRequired) {
                BeginInvoke(new Action<string>(Append), message);
                return;
            }
            log.AppendText("[" + DateTime.Now.ToString("HH:mm:ss") + "] " + message + Environment.NewLine);
        }

        private void Provision(Uri baseUri, string authorizationCode, string codeVerifier,
            string trustCaThumbprint, string serverFingerprint)
        {
            string host = Environment.MachineName.ToLowerInvariant();
            string fqdn = GetFqdn().ToLowerInvariant();
            string previousServerThumbprint = "";
            string previousCaThumbprint = "";
            string previousIssuerThumbprint = "";
            using (RegistryKey previous = Registry.LocalMachine.OpenSubKey(@"SOFTWARE\Kontrol\Kontrol-ID")) {
                if (previous != null) {
                    previousServerThumbprint = Convert.ToString(previous.GetValue("ServerCertificateThumbprint", ""));
                    previousCaThumbprint = Convert.ToString(previous.GetValue("CaCertificateThumbprint", ""));
                    previousIssuerThumbprint = Convert.ToString(previous.GetValue("IssuerThumbprint", ""));
                }
            }
            string working = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData),
                "Kontrol", "Kontrol-ID", "Enrollment", Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(working);
            try {
                Append("Gerando chave privada não exportável e requisição de certificado...");
                string infPath = Path.Combine(working, "request.inf");
                string csrPath = Path.Combine(working, "request.req");
                File.WriteAllText(infPath, BuildRequestInf(host, fqdn), new UTF8Encoding(false));
                RunProcess("certreq.exe", "-new -q -machine \"" + infPath + "\" \"" + csrPath + "\"", null, false);
                string csr = File.ReadAllText(csrPath);

                Append("Enviando CSR ao Kontrol com validação da CA e do hostname...");
                var serializer = new JavaScriptSerializer { MaxJsonLength = 131072 };
                string requestBody = serializer.Serialize(new Dictionary<string, object> {
                    { "authorizationCode", authorizationCode }, { "codeVerifier", codeVerifier },
                    { "hostname", host }, { "fqdn", fqdn }, { "csr", csr }
                });
                Uri endpoint = new Uri(baseUri.ToString().TrimEnd('/') + "/kontrol_id_enrollment.php");
                string responseBody = PostTrustedJson(endpoint, requestBody, trustCaThumbprint);
                var response = serializer.Deserialize<Dictionary<string, object>>(responseBody);
                if (!response.ContainsKey("ok") || !(response["ok"] is bool) || !(bool)response["ok"]) {
                    throw new InvalidOperationException(GetString(response, "error", "O Kontrol recusou a inscrição."));
                }

                string caPem = GetRequiredString(response, "caPem");
                string serverPem = GetRequiredString(response, "serverCertificatePem");
                string serverSha256 = NormalizeThumbprint(GetRequiredString(response, "serverCertificateSha256"));
                string clientSubject = GetRequiredString(response, "clientCertificateSubject");
                string issuer = NormalizeThumbprint(GetRequiredString(response, "clientCertificateIssuerThumbprint"));
                string resourceUri = GetRequiredString(response, "resourceUri");
                int winrmPort = Convert.ToInt32(response["winrmPort"]);
                string account = GetRequiredString(response, "localAccount");

                Append("Instalando a CA e associando o certificado à chave privada local...");
                var caCertificate = new X509Certificate2(Encoding.ASCII.GetBytes(caPem));
                if (!String.Equals(NormalizeThumbprint(caCertificate.Thumbprint),
                    NormalizeThumbprint(trustCaThumbprint), StringComparison.OrdinalIgnoreCase))
                    throw new InvalidOperationException("A API retornou uma CA diferente da CA incorporada no Provisioner.");
                AddCertificate(StoreName.Root, caCertificate);
                string certPath = Path.Combine(working, "station.cer");
                File.WriteAllText(certPath, serverPem, Encoding.ASCII);
                RunProcess("certreq.exe", "-accept -q -machine \"" + certPath + "\"", null, false);
                X509Certificate2 serverCertificate = FindCertificate(StoreName.My, serverSha256, "SHA256");
                if (serverCertificate == null || !serverCertificate.HasPrivateKey) {
                    throw new InvalidOperationException("O certificado da estação não foi associado à chave privada.");
                }

                // Persist the rollback manifest before changing WinRM or the local account.
                // If a later step fails, the Remove button can still unwind partial state.
                using (RegistryKey key = Registry.LocalMachine.CreateSubKey(@"SOFTWARE\Kontrol\Kontrol-ID")) {
                    key.SetValue("ServerCertificateThumbprint", serverCertificate.Thumbprint, RegistryValueKind.String);
                    key.SetValue("ServerCertificateSha256", serverSha256, RegistryValueKind.String);
                    key.SetValue("CaCertificateThumbprint", caCertificate.Thumbprint, RegistryValueKind.String);
                    key.SetValue("ClientSubject", clientSubject, RegistryValueKind.String);
                    key.SetValue("IssuerThumbprint", issuer, RegistryValueKind.String);
                    key.SetValue("ResourceUri", resourceUri, RegistryValueKind.String);
                    key.SetValue("LocalAccount", account, RegistryValueKind.String);
                    key.SetValue("WinRmPort", winrmPort, RegistryValueKind.DWord);
                    key.SetValue("Appliance", baseUri.GetLeftPart(UriPartial.Authority), RegistryValueKind.String);
                    key.SetValue("WebGuiFingerprint", serverFingerprint, RegistryValueKind.String);
                    key.SetValue("TrustCaThumbprint", trustCaThumbprint, RegistryValueKind.String);
                }

                Append("Criando conta técnica aleatória e aplicando restrições de logon...");
                string password = NativeMethods.GeneratePassword(40);
                NativeMethods.CreateOrUpdateLocalUser(account, password);
                NativeMethods.AddToRemoteManagementUsers(account);
                NativeMethods.AddDenyInteractiveRights(account);

                string[] applianceAddresses = ResolveAddresses(baseUri.DnsSafeHost);
                Append("Configurando WinRM HTTPS e firewall restrito ao Kontrol...");
                string script = BuildWinRmScript(host, serverCertificate.Thumbprint, clientSubject, issuer, previousIssuerThumbprint,
                    resourceUri, account, password, winrmPort, applianceAddresses);
                RunProcess("powershell.exe", "-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command -",
                    script, true);

                RemovePriorStationCertificates(serverCertificate.Thumbprint, caCertificate.Subject);
                if (!String.IsNullOrWhiteSpace(previousServerThumbprint) &&
                    !String.Equals(NormalizeThumbprint(previousServerThumbprint), NormalizeThumbprint(serverCertificate.Thumbprint), StringComparison.OrdinalIgnoreCase))
                    RemoveCertificate(StoreName.My, previousServerThumbprint);
                if (!String.IsNullOrWhiteSpace(previousCaThumbprint) &&
                    !String.Equals(NormalizeThumbprint(previousCaThumbprint), NormalizeThumbprint(caCertificate.Thumbprint), StringComparison.OrdinalIgnoreCase))
                    RemoveCertificate(StoreName.Root, previousCaThumbprint);

                Append("A chave privada permaneceu no repositório LocalMachine e não é exportável.");
            } finally {
                try { Directory.Delete(working, true); } catch { }
            }
        }

        private void RemoveProvisioning()
        {
            using (RegistryKey key = Registry.LocalMachine.OpenSubKey(@"SOFTWARE\Kontrol\Kontrol-ID")) {
                if (key == null) {
                    throw new InvalidOperationException("Esta estação não possui uma inscrição Kontrol-ID registrada.");
                }
                string certThumb = Convert.ToString(key.GetValue("ServerCertificateThumbprint", ""));
                string caThumb = Convert.ToString(key.GetValue("CaCertificateThumbprint", ""));
                string subject = Convert.ToString(key.GetValue("ClientSubject", ""));
                string resource = Convert.ToString(key.GetValue("ResourceUri", ""));
                string account = Convert.ToString(key.GetValue("LocalAccount", "KontrolIdentity"));
                string applianceUrl = Convert.ToString(key.GetValue("Appliance", ""));
                string webGuiFingerprint = NormalizeThumbprint(Convert.ToString(key.GetValue("WebGuiFingerprint", "")));
                string trustCaThumbprint = NormalizeThumbprint(Convert.ToString(key.GetValue("TrustCaThumbprint", "")));
                RequestRemoteUnenrollment(applianceUrl, webGuiFingerprint, trustCaThumbprint, certThumb);
                string script = BuildRemovalScript(certThumb, subject, resource, caThumb, account);
                RunProcess("powershell.exe", "-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command -",
                    script, true);
                RemoveCertificate(StoreName.My, certThumb);
                RemoveCertificate(StoreName.Root, caThumb);
                NativeMethods.DeleteLocalUser(account);
            }
            Registry.LocalMachine.DeleteSubKeyTree(@"SOFTWARE\Kontrol\Kontrol-ID", false);
        }

        private void RequestRemoteUnenrollment(string applianceUrl, string webGuiFingerprint,
            string trustCaThumbprint, string certThumbprint)
        {
            Uri baseUri;
            if (!Uri.TryCreate(applianceUrl, UriKind.Absolute, out baseUri) ||
                !String.Equals(baseUri.Scheme, Uri.UriSchemeHttps, StringComparison.OrdinalIgnoreCase)) {
                throw new InvalidOperationException(
                    "A matrícula local não contém um endereço HTTPS válido para revogação no Kontrol.");
            }
            if (webGuiFingerprint.Length != 64 || webGuiFingerprint.Any(c => !Uri.IsHexDigit(c)))
                webGuiFingerprint = "";
            X509Certificate2 certificate = FindCertificate(StoreName.My, certThumbprint, "SHA1");
            if (certificate == null || !certificate.HasPrivateKey)
                throw new InvalidOperationException("O certificado privado da estação não está disponível para autorizar a desmatrícula.");

            string hostname = Environment.MachineName.ToLowerInvariant();
            long timestamp = UnixNow();
            byte[] nonceBytes = new byte[32];
            byte[] signature = null;
            try {
                using (RandomNumberGenerator random = RandomNumberGenerator.Create()) random.GetBytes(nonceBytes);
                string nonce = Convert.ToBase64String(nonceBytes);
                string fingerprint;
                using (SHA256 sha = SHA256.Create())
                    fingerprint = BitConverter.ToString(sha.ComputeHash(certificate.RawData)).Replace("-", "");
                string signedPayload = "kontrol-id-unenroll-v1\n" + hostname + "\n" + timestamp + "\n" + nonce + "\n" + fingerprint;
                using (RSA privateKey = certificate.GetRSAPrivateKey()) {
                    if (privateKey == null) throw new InvalidOperationException("A chave privada da estação não é RSA.");
                    signature = privateKey.SignData(Encoding.UTF8.GetBytes(signedPayload), HashAlgorithmName.SHA256,
                        RSASignaturePadding.Pkcs1);
                }
                string certificatePem = "-----BEGIN CERTIFICATE-----\n" +
                    Convert.ToBase64String(certificate.RawData, Base64FormattingOptions.InsertLineBreaks) +
                    "\n-----END CERTIFICATE-----\n";
                var serializer = new JavaScriptSerializer { MaxJsonLength = 65536 };
                string requestBody = serializer.Serialize(new Dictionary<string, object> {
                    { "hostname", hostname }, { "timestamp", timestamp }, { "nonce", nonce },
                    { "certificate", certificatePem }, { "signature", Convert.ToBase64String(signature) }
                });
                Uri endpoint = new Uri(baseUri.ToString().TrimEnd('/') + "/kontrol_id_unenrollment.php");
                Append("Revogando a matrícula no Kontrol...");
                string responseBody = trustCaThumbprint.Length == 40
                    ? PostTrustedJson(endpoint, requestBody, trustCaThumbprint)
                    : PostPinnedJson(endpoint, requestBody, webGuiFingerprint);
                var response = serializer.Deserialize<Dictionary<string, object>>(responseBody);
                if (!response.ContainsKey("ok") || !(response["ok"] is bool) || !(bool)response["ok"])
                    throw new InvalidOperationException(GetString(response, "error", "O Kontrol recusou a desmatrícula."));
            } finally {
                Array.Clear(nonceBytes, 0, nonceBytes.Length);
                if (signature != null) Array.Clear(signature, 0, signature.Length);
            }
        }

        private static string BuildRequestInf(string hostname, string fqdn)
        {
            var san = new List<string> { "dns=" + hostname };
            if (!String.IsNullOrWhiteSpace(fqdn) && !String.Equals(hostname, fqdn, StringComparison.OrdinalIgnoreCase))
                san.Add("dns=" + fqdn);
            san.AddRange(LocalIpv4Addresses().Select(ip => "ipaddress=" + ip));
            return "[Version]\r\nSignature=\"$Windows NT$\"\r\n\r\n" +
                "[NewRequest]\r\nSubject=\"CN=" + hostname + "\"\r\nKeyAlgorithm=RSA\r\nKeyLength=3072\r\n" +
                "MachineKeySet=TRUE\r\nExportable=FALSE\r\nRequestType=PKCS10\r\nHashAlgorithm=sha256\r\n" +
                "KeyUsage=0xa0\r\nProviderName=\"Microsoft Software Key Storage Provider\"\r\n\r\n" +
                "[Extensions]\r\n2.5.29.17=\"{text}\"\r\n_continue_=\"" + String.Join("&", san) + "\"\r\n";
        }

        private static string PostPinnedJson(Uri endpoint, string json, string requiredPin)
        {
            ServicePointManager.SecurityProtocol = SecurityProtocolType.Tls12;
            RemoteCertificateValidationCallback previous = ServicePointManager.ServerCertificateValidationCallback;
            ServicePointManager.ServerCertificateValidationCallback = (sender, certificate, chain, errors) => {
                if (certificate == null) return false;
                if (String.IsNullOrWhiteSpace(requiredPin)) return errors == SslPolicyErrors.None;
                using (var sha = SHA256.Create()) {
                    string observed = BitConverter.ToString(sha.ComputeHash(certificate.GetRawCertData())).Replace("-", "");
                    return String.Equals(observed, requiredPin, StringComparison.OrdinalIgnoreCase);
                }
            };
            try {
                byte[] payload = Encoding.UTF8.GetBytes(json);
                var request = (HttpWebRequest)WebRequest.Create(endpoint);
                request.Method = "POST";
                request.ContentType = "application/json";
                request.Accept = "application/json";
                request.ContentLength = payload.Length;
                request.Timeout = 20000;
                request.ReadWriteTimeout = 20000;
                request.AllowAutoRedirect = false;
                using (Stream stream = request.GetRequestStream()) stream.Write(payload, 0, payload.Length);
                try {
                    using (var response = (HttpWebResponse)request.GetResponse())
                    using (var reader = new StreamReader(response.GetResponseStream(), Encoding.UTF8))
                        return reader.ReadToEnd();
                } catch (WebException ex) {
                    var response = ex.Response as HttpWebResponse;
                    if (response != null) {
                        using (response)
                        using (var reader = new StreamReader(response.GetResponseStream(), Encoding.UTF8))
                            throw new InvalidOperationException("Kontrol respondeu HTTP " + (int)response.StatusCode + ": " + reader.ReadToEnd());
                    }
                    if (ex.Status == WebExceptionStatus.Timeout) {
                        throw new InvalidOperationException(
                            "A estação não conseguiu alcançar " + endpoint.GetLeftPart(UriPartial.Authority) +
                            " por HTTPS dentro de 20 segundos. Verifique a rota e o firewall entre a estação e o Kontrol.", ex);
                    }
                    if (ex.Status == WebExceptionStatus.ConnectFailure ||
                        ex.Status == WebExceptionStatus.NameResolutionFailure ||
                        ex.Status == WebExceptionStatus.ProxyNameResolutionFailure) {
                        throw new InvalidOperationException(
                            "A estação não conseguiu conectar a " + endpoint.GetLeftPart(UriPartial.Authority) +
                            " por HTTPS. Verifique endereço, DNS, rota e firewall.", ex);
                    }
                    throw new InvalidOperationException(
                        "Falha na comunicação HTTPS com o Kontrol (" + ex.Status + "): " + ex.Message, ex);
                }
            } finally {
                ServicePointManager.ServerCertificateValidationCallback = previous;
            }
        }

        private static string PostTrustedJson(Uri endpoint, string json, string trustCaThumbprint)
        {
            string requiredRoot = NormalizeThumbprint(trustCaThumbprint);
            if (requiredRoot.Length != 40 || requiredRoot.Any(c => !Uri.IsHexDigit(c)))
                throw new InvalidOperationException("A CA incorporada no Provisioner é inválida.");
            ServicePointManager.SecurityProtocol = SecurityProtocolType.Tls12;
            RemoteCertificateValidationCallback previous = ServicePointManager.ServerCertificateValidationCallback;
            ServicePointManager.ServerCertificateValidationCallback = (sender, certificate, chain, errors) => {
                if (certificate == null || chain == null || errors != SslPolicyErrors.None || chain.ChainElements.Count == 0)
                    return false;
                X509Certificate2 root = chain.ChainElements[chain.ChainElements.Count - 1].Certificate;
                return String.Equals(NormalizeThumbprint(root.Thumbprint), requiredRoot,
                    StringComparison.OrdinalIgnoreCase);
            };
            try {
                return PostJsonWithoutCertificateOverride(endpoint, json);
            } finally {
                ServicePointManager.ServerCertificateValidationCallback = previous;
            }
        }

        private static string PostJsonWithoutCertificateOverride(Uri endpoint, string json)
        {
            byte[] payload = Encoding.UTF8.GetBytes(json);
            try {
                var request = (HttpWebRequest)WebRequest.Create(endpoint);
                request.Method = "POST";
                request.ContentType = "application/json";
                request.Accept = "application/json";
                request.ContentLength = payload.Length;
                request.Timeout = 20000;
                request.ReadWriteTimeout = 20000;
                request.AllowAutoRedirect = false;
                using (Stream stream = request.GetRequestStream()) stream.Write(payload, 0, payload.Length);
                try {
                    using (var response = (HttpWebResponse)request.GetResponse())
                    using (var reader = new StreamReader(response.GetResponseStream(), Encoding.UTF8))
                        return reader.ReadToEnd();
                } catch (WebException ex) {
                    var response = ex.Response as HttpWebResponse;
                    if (response != null) {
                        using (response)
                        using (var reader = new StreamReader(response.GetResponseStream(), Encoding.UTF8)) {
                            string responseText = reader.ReadToEnd();
                            if (response.StatusCode == HttpStatusCode.Unauthorized)
                                throw new InvalidOperationException("Usuário ou senha inválidos.");
                            throw new InvalidOperationException("Kontrol respondeu HTTP " +
                                (int)response.StatusCode + ": " + responseText);
                        }
                    }
                    if (ex.Status == WebExceptionStatus.TrustFailure)
                        throw new InvalidOperationException(
                            "O certificado HTTPS do Kontrol não foi emitido pela CA incorporada ou não corresponde ao endereço informado.", ex);
                    if (ex.Status == WebExceptionStatus.Timeout)
                        throw new InvalidOperationException(
                            "A estação não conseguiu alcançar " + endpoint.GetLeftPart(UriPartial.Authority) +
                            " por HTTPS dentro de 20 segundos. Verifique a rota e o firewall.", ex);
                    throw new InvalidOperationException(
                        "Falha na comunicação HTTPS com o Kontrol (" + ex.Status + "): " + ex.Message, ex);
                }
            } finally {
                Array.Clear(payload, 0, payload.Length);
            }
        }

        private static string BuildWinRmScript(string hostname, string certThumb, string subject,
            string issuer, string previousIssuer, string resource, string account, string password, int port, string[] applianceAddresses)
        {
            return "$ErrorActionPreference='Stop'\n" +
                "$hostName='" + Ps(hostname) + "'\n$thumb='" + Ps(certThumb) + "'\n" +
                "$subject='" + Ps(subject) + "'\n$issuer='" + Ps(issuer) + "'\n$previousIssuer='" + Ps(previousIssuer) + "'\n" +
                "$resource='" + Ps(resource) + "'\n$account='" + Ps(account) + "'\n" +
                "$password='" + Ps(password) + "'\n$port=" + port + "\n" +
                "$remote=@(" + String.Join(",", applianceAddresses.Select(a => "'" + Ps(a) + "'")) + ")\n" +
                "Set-Service -Name WinRM -StartupType Automatic\nStart-Service -Name WinRM\n" +
                "$httpsListeners=@(Get-ChildItem WSMan:\\localhost\\Listener | Where-Object { $_.Keys -contains 'Transport=HTTPS' })\n" +
                "$httpsListeners | Where-Object { (Get-Item -LiteralPath ($_.PSPath + '\\CertificateThumbprint')).Value -ne $thumb } | Remove-Item -Recurse -Force\n" +
                "if (-not (Get-ChildItem WSMan:\\localhost\\Listener | Where-Object { $_.Keys -contains 'Transport=HTTPS' -and (Get-Item -LiteralPath ($_.PSPath + '\\CertificateThumbprint')).Value -eq $thumb })) { New-Item -Path WSMan:\\localhost\\Listener -Address * -Transport HTTPS -Hostname $hostName -CertificateThumbPrint $thumb -Port $port -Force | Out-Null }\n" +
                "Set-Item WSMan:\\localhost\\Service\\Auth\\Certificate -Value $true\n" +
                "Set-Item WSMan:\\localhost\\Service\\Auth\\Basic -Value $false\n" +
                "Set-Item WSMan:\\localhost\\Service\\AllowUnencrypted -Value $false\n" +
                "$secure=ConvertTo-SecureString $password -AsPlainText -Force\n$credential=New-Object System.Management.Automation.PSCredential(\"$env:COMPUTERNAME\\$account\",$secure)\n" +
                "Get-ChildItem WSMan:\\localhost\\ClientCertificate | Where-Object { $_.Keys -contains ('Issuer=' + $issuer) -or ($previousIssuer -and $_.Keys -contains ('Issuer=' + $previousIssuer)) } | Remove-Item -Recurse -Force\n" +
                "New-Item -Path WSMan:\\localhost\\ClientCertificate -Subject $subject -URI $resource -Issuer $issuer -Credential $credential -Force | Out-Null\n" +
                "$accountSid=(New-Object System.Security.Principal.NTAccount($env:COMPUTERNAME,$account)).Translate([System.Security.Principal.SecurityIdentifier])\n" +
                "$security=[wmiclass]'\\\\.\\root\\cimv2:__SystemSecurity'\n$current=$security.GetSD()\n" +
                "if ($current.ReturnValue -ne 0) { throw \"WMI GetSD falhou ($($current.ReturnValue)).\" }\n" +
                "$descriptor=[System.Security.AccessControl.RawSecurityDescriptor]::new([byte[]]$current.SD,0)\n" +
                "$kept=@($descriptor.DiscretionaryAcl | Where-Object { -not (($_ -is [System.Security.AccessControl.KnownAce]) -and $_.SecurityIdentifier.Value -eq $accountSid.Value) })\n" +
                "$acl=[System.Security.AccessControl.RawAcl]::new($descriptor.DiscretionaryAcl.Revision,$kept.Count+1)\n" +
                "foreach ($entry in $kept) { $acl.InsertAce($acl.Count,$entry) }\n" +
                "$ace=[System.Security.AccessControl.CommonAce]::new([System.Security.AccessControl.AceFlags]::None,[System.Security.AccessControl.AceQualifier]::AccessAllowed,33,$accountSid,$false,$null)\n" +
                "$acl.InsertAce($acl.Count,$ace)\n$descriptor.DiscretionaryAcl=$acl\n" +
                "$bytes=New-Object byte[] $descriptor.BinaryLength\n$descriptor.GetBinaryForm($bytes,0)\n$updated=$security.SetSD($bytes)\n" +
                "if ($updated.ReturnValue -ne 0) { throw \"WMI SetSD falhou ($($updated.ReturnValue)).\" }\n" +
                "$rule=Get-NetFirewallRule -DisplayName 'Kontrol-ID WinRM HTTPS' -ErrorAction SilentlyContinue\n" +
                "if ($rule) { Remove-NetFirewallRule -DisplayName 'Kontrol-ID WinRM HTTPS' }\n" +
                "New-NetFirewallRule -DisplayName 'Kontrol-ID WinRM HTTPS' -Direction Inbound -Action Allow -Protocol TCP -LocalPort $port -RemoteAddress $remote -Profile Any | Out-Null\n" +
                "Restart-Service -Name WinRM\n";
        }

        private static string BuildRemovalScript(string certThumb, string subject, string resource, string issuer, string account)
        {
            return "$ErrorActionPreference='Stop'\n$thumb='" + Ps(certThumb) + "'\n$subject='" + Ps(subject) + "'\n$resource='" + Ps(resource) + "'\n$issuer='" + Ps(issuer) + "'\n$account='" + Ps(account) + "'\n" +
                "Get-ChildItem WSMan:\\localhost\\ClientCertificate -ErrorAction SilentlyContinue | Where-Object { $_.Keys -contains ('Issuer=' + $issuer) } | Remove-Item -Recurse -Force\n" +
                "Get-ChildItem WSMan:\\localhost\\Listener -ErrorAction SilentlyContinue | Where-Object { $_.Keys -contains 'Transport=HTTPS' -and (Get-Item -LiteralPath ($_.PSPath + '\\CertificateThumbprint')).Value -eq $thumb } | Remove-Item -Recurse -Force\n" +
                "$accountSid=(New-Object System.Security.Principal.NTAccount($env:COMPUTERNAME,$account)).Translate([System.Security.Principal.SecurityIdentifier])\n" +
                "$security=[wmiclass]'\\\\.\\root\\cimv2:__SystemSecurity'\n$current=$security.GetSD()\n" +
                "if ($current.ReturnValue -ne 0) { throw \"WMI GetSD falhou ($($current.ReturnValue)).\" }\n" +
                "$descriptor=[System.Security.AccessControl.RawSecurityDescriptor]::new([byte[]]$current.SD,0)\n" +
                "$kept=@($descriptor.DiscretionaryAcl | Where-Object { -not (($_ -is [System.Security.AccessControl.KnownAce]) -and $_.SecurityIdentifier.Value -eq $accountSid.Value) })\n" +
                "$acl=[System.Security.AccessControl.RawAcl]::new($descriptor.DiscretionaryAcl.Revision,$kept.Count)\n" +
                "foreach ($entry in $kept) { $acl.InsertAce($acl.Count,$entry) }\n$descriptor.DiscretionaryAcl=$acl\n" +
                "$bytes=New-Object byte[] $descriptor.BinaryLength\n$descriptor.GetBinaryForm($bytes,0)\n$updated=$security.SetSD($bytes)\n" +
                "if ($updated.ReturnValue -ne 0) { throw \"WMI SetSD falhou ($($updated.ReturnValue)).\" }\n" +
                "Get-NetFirewallRule -DisplayName 'Kontrol-ID WinRM HTTPS' -ErrorAction SilentlyContinue | Remove-NetFirewallRule\n";
        }

        private static string Ps(string value) { return (value ?? "").Replace("'", "''"); }

        private static void AddCertificate(StoreName name, X509Certificate2 cert)
        {
            using (var store = new X509Store(name, StoreLocation.LocalMachine)) {
                store.Open(OpenFlags.ReadWrite);
                if (store.Certificates.Find(X509FindType.FindByThumbprint, cert.Thumbprint, false).Count == 0)
                    store.Add(cert);
            }
        }

        private static X509Certificate2 FindCertificate(StoreName name, string value, string hash)
        {
            using (var store = new X509Store(name, StoreLocation.LocalMachine)) {
                store.Open(OpenFlags.ReadOnly);
                foreach (X509Certificate2 cert in store.Certificates) {
                    string observed;
                    if (hash == "SHA256") using (var sha = SHA256.Create())
                        observed = BitConverter.ToString(sha.ComputeHash(cert.RawData)).Replace("-", "");
                    else observed = NormalizeThumbprint(cert.Thumbprint);
                    if (String.Equals(observed, value, StringComparison.OrdinalIgnoreCase)) return cert;
                }
            }
            return null;
        }

        private static void RemoveCertificate(StoreName name, string thumbprint)
        {
            if (String.IsNullOrWhiteSpace(thumbprint)) return;
            using (var store = new X509Store(name, StoreLocation.LocalMachine)) {
                store.Open(OpenFlags.ReadWrite);
                foreach (X509Certificate2 cert in store.Certificates.Find(X509FindType.FindByThumbprint,
                    NormalizeThumbprint(thumbprint), false)) {
                    RSA privateKey = null;
                    try { privateKey = cert.GetRSAPrivateKey(); } catch (CryptographicException) { }
                    store.Remove(cert);
                    try {
                        var cng = privateKey as RSACng;
                        if (cng != null) cng.Key.Delete();
                    } catch (CryptographicException) { }
                    if (privateKey != null) privateKey.Dispose();
                }
            }
        }

        private static void RemovePriorStationCertificates(string currentThumbprint, string issuerName)
        {
            using (var store = new X509Store(StoreName.My, StoreLocation.LocalMachine)) {
                store.Open(OpenFlags.ReadOnly);
                var stale = store.Certificates.Cast<X509Certificate2>().Where(cert =>
                    !String.Equals(NormalizeThumbprint(cert.Thumbprint), NormalizeThumbprint(currentThumbprint), StringComparison.OrdinalIgnoreCase) &&
                    String.Equals(cert.Issuer, issuerName, StringComparison.OrdinalIgnoreCase)).Select(cert => cert.Thumbprint).ToArray();
                store.Close();
                foreach (string thumbprint in stale) RemoveCertificate(StoreName.My, thumbprint);
            }
        }

        private static string[] ResolveAddresses(string host)
        {
            IPAddress literal;
            if (IPAddress.TryParse(host, out literal)) return new[] { literal.ToString() };
            var addresses = Dns.GetHostAddresses(host).Where(a => a.AddressFamily == System.Net.Sockets.AddressFamily.InterNetwork ||
                a.AddressFamily == System.Net.Sockets.AddressFamily.InterNetworkV6).Select(a => a.ToString()).Distinct().ToArray();
            if (addresses.Length == 0) throw new InvalidOperationException("Não foi possível resolver o endereço do Kontrol.");
            return addresses;
        }

        private static IEnumerable<string> LocalIpv4Addresses()
        {
            return NetworkInterface.GetAllNetworkInterfaces()
                .Where(n => n.OperationalStatus == OperationalStatus.Up && n.NetworkInterfaceType != NetworkInterfaceType.Loopback)
                .SelectMany(n => n.GetIPProperties().UnicastAddresses)
                .Where(a => a.Address.AddressFamily == System.Net.Sockets.AddressFamily.InterNetwork)
                .Select(a => a.Address.ToString()).Where(a => !a.StartsWith("169.254.")).Distinct();
        }

        private static string GetFqdn()
        {
            try { return Dns.GetHostEntry(Environment.MachineName).HostName; }
            catch { return Environment.MachineName; }
        }

        private static string GetRequiredString(Dictionary<string, object> data, string key)
        {
            string value = GetString(data, key, "");
            if (String.IsNullOrWhiteSpace(value)) throw new InvalidOperationException("Resposta incompleta do Kontrol: " + key);
            return value;
        }

        private static string GetString(Dictionary<string, object> data, string key, string fallback)
        {
            object value;
            return data.TryGetValue(key, out value) && value != null ? Convert.ToString(value) : fallback;
        }

        private static string NormalizeThumbprint(string value)
        {
            return new string((value ?? "").Where(Uri.IsHexDigit).ToArray()).ToUpperInvariant();
        }

        private static string RunProcess(string file, string arguments, string standardInput, bool hideOutput)
        {
            var start = new ProcessStartInfo(file, arguments) {
                UseShellExecute = false, CreateNoWindow = true, RedirectStandardOutput = true,
                RedirectStandardError = true, RedirectStandardInput = standardInput != null,
                StandardOutputEncoding = Encoding.UTF8, StandardErrorEncoding = Encoding.UTF8
            };
            using (var process = Process.Start(start)) {
                if (standardInput != null) { process.StandardInput.Write(standardInput); process.StandardInput.Close(); }
                string stdout = process.StandardOutput.ReadToEnd();
                string stderr = process.StandardError.ReadToEnd();
                process.WaitForExit();
                if (process.ExitCode != 0)
                    throw new InvalidOperationException(Path.GetFileName(file) + " falhou (" + process.ExitCode + "): " + (hideOutput ? "consulte o log de eventos do Windows" : stderr + " " + stdout));
                return stdout;
            }
        }
    }

    internal static class NativeMethods
    {
        private const int NERR_Success = 0;
        private const int NERR_UserExists = 2224;
        private const uint UF_SCRIPT = 0x0001;
        private const uint UF_DONT_EXPIRE_PASSWD = 0x10000;
        private const uint USER_PRIV_USER = 1;
        private const uint POLICY_LOOKUP_NAMES = 0x00000800;
        private const uint POLICY_CREATE_ACCOUNT = 0x00000010;

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        private struct USER_INFO_1 { public string name; public string password; public uint password_age; public uint priv; public string home_dir; public string comment; public uint flags; public string script_path; }
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        private struct USER_INFO_1003 { public string password; }
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        private struct USER_INFO_1008 { public uint flags; }
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        private struct LOCALGROUP_MEMBERS_INFO_3 { public string domainandname; }
        [StructLayout(LayoutKind.Sequential)]
        private struct LSA_OBJECT_ATTRIBUTES { public uint Length; public IntPtr RootDirectory; public IntPtr ObjectName; public uint Attributes; public IntPtr SecurityDescriptor; public IntPtr SecurityQualityOfService; }
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        private struct LSA_UNICODE_STRING { public ushort Length; public ushort MaximumLength; public IntPtr Buffer; }

        [DllImport("Netapi32.dll", CharSet = CharSet.Unicode)] private static extern int NetUserAdd(string servername, int level, ref USER_INFO_1 buf, out uint parm_err);
        [DllImport("Netapi32.dll", CharSet = CharSet.Unicode)] private static extern int NetUserSetInfo(string servername, string username, int level, IntPtr buf, out uint parm_err);
        [DllImport("Netapi32.dll", CharSet = CharSet.Unicode)] private static extern int NetUserDel(string servername, string username);
        [DllImport("Netapi32.dll", CharSet = CharSet.Unicode)] private static extern int NetLocalGroupAddMembers(string servername, string groupname, int level, ref LOCALGROUP_MEMBERS_INFO_3 buf, int totalentries);
        [DllImport("advapi32.dll", SetLastError = true)] private static extern bool LookupAccountName(string systemName, string accountName, IntPtr sid, ref uint sidSize, StringBuilder domainName, ref uint domainSize, out int use);
        [DllImport("advapi32.dll")] private static extern uint LsaOpenPolicy(IntPtr systemName, ref LSA_OBJECT_ATTRIBUTES attributes, uint access, out IntPtr handle);
        [DllImport("advapi32.dll")] private static extern uint LsaAddAccountRights(IntPtr handle, IntPtr sid, LSA_UNICODE_STRING[] rights, int count);
        [DllImport("advapi32.dll")] private static extern uint LsaClose(IntPtr handle);

        public static bool IsAdministrator()
        {
            using (WindowsIdentity identity = WindowsIdentity.GetCurrent())
                return new WindowsPrincipal(identity).IsInRole(WindowsBuiltInRole.Administrator);
        }

        public static string GeneratePassword(int length)
        {
            const string alphabet = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789!#%+,-.:=@_";
            byte[] bytes = new byte[length];
            using (var rng = RandomNumberGenerator.Create()) rng.GetBytes(bytes);
            var chars = new char[length];
            for (int i = 0; i < length; i++) chars[i] = alphabet[bytes[i] % alphabet.Length];
            chars[0] = 'K'; chars[1] = 'i'; chars[2] = '9'; chars[3] = '!';
            return new string(chars.OrderBy(c => Guid.NewGuid()).ToArray());
        }

        public static void CreateOrUpdateLocalUser(string username, string password)
        {
            var info = new USER_INFO_1 { name = username, password = password, priv = USER_PRIV_USER,
                comment = "Conta técnica restrita do Kontrol-ID", flags = UF_SCRIPT | UF_DONT_EXPIRE_PASSWD };
            uint parameter;
            int result = NetUserAdd(null, 1, ref info, out parameter);
            if (result != NERR_Success && result != NERR_UserExists)
                throw new InvalidOperationException("Não foi possível criar a conta técnica (NetUserAdd " + result + ").");
            if (result == NERR_UserExists) SetUserInfo(username, 1003, new USER_INFO_1003 { password = password }, out parameter);
            SetUserInfo(username, 1008, new USER_INFO_1008 { flags = UF_SCRIPT | UF_DONT_EXPIRE_PASSWD }, out parameter);
        }

        private static void SetUserInfo<T>(string username, int level, T info, out uint parameter)
        {
            IntPtr buffer = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(T)));
            try {
                Marshal.StructureToPtr(info, buffer, false);
                int result = NetUserSetInfo(null, username, level, buffer, out parameter);
                if (result != NERR_Success) throw new InvalidOperationException("NetUserSetInfo falhou (" + result + ").");
            } finally { Marshal.FreeHGlobal(buffer); }
        }

        public static void AddToRemoteManagementUsers(string username)
        {
            string translated = new SecurityIdentifier("S-1-5-32-580").Translate(typeof(NTAccount)).Value;
            string group = translated.Contains("\\") ? translated.Substring(translated.IndexOf('\\') + 1) : translated;
            var member = new LOCALGROUP_MEMBERS_INFO_3 { domainandname = Environment.MachineName + "\\" + username };
            int result = NetLocalGroupAddMembers(null, group, 3, ref member, 1);
            if (result != NERR_Success && result != 1378)
                throw new InvalidOperationException("Não foi possível restringir a conta ao grupo de gerenciamento remoto (" + result + ").");
        }

        public static void AddDenyInteractiveRights(string username)
        {
            string account = Environment.MachineName + "\\" + username;
            uint sidSize = 0, domainSize = 0; int use;
            LookupAccountName(null, account, IntPtr.Zero, ref sidSize, null, ref domainSize, out use);
            IntPtr sid = Marshal.AllocHGlobal((int)sidSize);
            var domain = new StringBuilder((int)domainSize);
            try {
                if (!LookupAccountName(null, account, sid, ref sidSize, domain, ref domainSize, out use))
                    throw new InvalidOperationException("Não foi possível localizar o SID da conta técnica.");
                var attributes = new LSA_OBJECT_ATTRIBUTES { Length = (uint)Marshal.SizeOf(typeof(LSA_OBJECT_ATTRIBUTES)) };
                IntPtr policy;
                uint status = LsaOpenPolicy(IntPtr.Zero, ref attributes, POLICY_LOOKUP_NAMES | POLICY_CREATE_ACCOUNT, out policy);
                if (status != 0) throw new InvalidOperationException("LsaOpenPolicy falhou (" + status + ").");
                try {
                    var rights = new[] { MakeLsaString("SeDenyInteractiveLogonRight"), MakeLsaString("SeDenyRemoteInteractiveLogonRight") };
                    try {
                        status = LsaAddAccountRights(policy, sid, rights, rights.Length);
                        if (status != 0) throw new InvalidOperationException("LsaAddAccountRights falhou (" + status + ").");
                    } finally { foreach (var right in rights) Marshal.FreeHGlobal(right.Buffer); }
                } finally { LsaClose(policy); }
            } finally { Marshal.FreeHGlobal(sid); }
        }

        private static LSA_UNICODE_STRING MakeLsaString(string value)
        {
            return new LSA_UNICODE_STRING { Buffer = Marshal.StringToHGlobalUni(value), Length = (ushort)(value.Length * 2), MaximumLength = (ushort)((value.Length + 1) * 2) };
        }

        public static void DeleteLocalUser(string username)
        {
            int result = NetUserDel(null, username);
            if (result != NERR_Success && result != 2221)
                throw new InvalidOperationException("Não foi possível remover a conta técnica (" + result + ").");
        }
    }
}
