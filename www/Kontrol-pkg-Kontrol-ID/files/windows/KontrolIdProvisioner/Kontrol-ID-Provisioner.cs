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
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Security.Cryptography.X509Certificates;
using System.Security.Principal;
using System.Text;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows.Forms;

namespace Kontrol.Identity.Provisioner
{
    internal static class Program
    {
        [STAThread]
        private static void Main(string[] args)
        {
            if (args.Length == 2 && String.Equals(args[0], "--install-config", StringComparison.OrdinalIgnoreCase)) {
                RunHeadlessInstall(args[1]);
                return;
            }
            if (args.Length == 2 && String.Equals(args[0], "--remove-config", StringComparison.OrdinalIgnoreCase)) {
                RunHeadlessRemove(args[1]);
                return;
            }
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new ProvisionerForm());
        }

        private static void RunHeadlessInstall(string configPath)
        {
            string resultPath = configPath + ".result.json";
            var serializer = new JavaScriptSerializer();
            var result = new Dictionary<string, object>();
            try {
                var settings = serializer.Deserialize<Dictionary<string, object>>(File.ReadAllText(configPath, Encoding.UTF8));
                object configuredResult;
                if (settings.TryGetValue("resultPath", out configuredResult) && configuredResult != null)
                    resultPath = Convert.ToString(configuredResult);
                if (!NativeMethods.IsAdministrator())
                    throw new InvalidOperationException("O provisionamento silencioso requer privilégios administrativos.");
                var form = new ProvisionerForm();
                form.ProvisionForAutomation(new Uri(Convert.ToString(settings["appliance"])),
                    Convert.ToString(settings["token"]), Convert.ToString(settings["fingerprint"]));
                result["ok"] = true;
                result["message"] = "Kontrol-ID provisioned successfully.";
                Environment.ExitCode = 0;
            } catch (Exception ex) {
                result["ok"] = false;
                result["error"] = ex.Message;
                Environment.ExitCode = 1;
            } finally {
                try { File.WriteAllText(resultPath, serializer.Serialize(result), new UTF8Encoding(false)); } catch { }
                try { File.Delete(configPath); } catch { }
            }
        }

        private static void RunHeadlessRemove(string configPath)
        {
            string resultPath = configPath + ".result.json";
            var serializer = new JavaScriptSerializer();
            var result = new Dictionary<string, object>();
            try {
                var settings = serializer.Deserialize<Dictionary<string, object>>(File.ReadAllText(configPath, Encoding.UTF8));
                object configuredResult;
                if (settings.TryGetValue("resultPath", out configuredResult) && configuredResult != null)
                    resultPath = Convert.ToString(configuredResult);
                if (!NativeMethods.IsAdministrator())
                    throw new InvalidOperationException("A remoção silenciosa requer privilégios administrativos.");
                var form = new ProvisionerForm();
                form.RemoveForAutomation();
                result["ok"] = true;
                result["message"] = "Kontrol-ID configuration removed successfully.";
                Environment.ExitCode = 0;
            } catch (Exception ex) {
                result["ok"] = false;
                result["error"] = ex.Message;
                Environment.ExitCode = 1;
            } finally {
                try { File.WriteAllText(resultPath, serializer.Serialize(result), new UTF8Encoding(false)); } catch { }
                try { File.Delete(configPath); } catch { }
            }
        }
    }

    internal sealed class ProvisionerForm : Form
    {
        private readonly TextBox appliance = new TextBox();
        private readonly TextBox token = new TextBox();
        private readonly TextBox fingerprint = new TextBox();
        private readonly TextBox log = new TextBox();
        private readonly Button install = new Button();
        private readonly Button remove = new Button();

        public ProvisionerForm()
        {
            Text = "Kontrol-ID - Configurar estação";
            StartPosition = FormStartPosition.CenterScreen;
            MinimumSize = new Size(690, 590);
            Size = new Size(760, 650);
            Font = new Font("Segoe UI", 9.5F);
            BackColor = Color.FromArgb(242, 246, 249);

            var title = new Label {
                Text = "Kontrol-ID",
                Font = new Font("Segoe UI Semibold", 24F),
                ForeColor = Color.FromArgb(17, 38, 61),
                AutoSize = true,
                Location = new Point(34, 25)
            };
            var subtitle = new Label {
                Text = "Inscrição segura desta estação Windows, sem instalar serviço residente.",
                ForeColor = Color.FromArgb(80, 98, 114),
                AutoSize = true,
                Location = new Point(38, 72)
            };
            Controls.Add(title);
            Controls.Add(subtitle);

            AddField("Endereço HTTPS do Kontrol", appliance, 112,
                "Exemplo: https://192.168.1.1");
            AddField("Token temporário de inscrição", token, 180,
                "Gerado em Serviços > Kontrol-ID > Estações e sessões");
            AddField("SHA-256 do certificado da WebGUI", fingerprint, 248,
                "Confirme este valor na GUI do Kontrol antes de continuar");

            install.Text = "Configurar estação";
            install.Location = new Point(38, 325);
            install.Size = new Size(205, 39);
            install.BackColor = Color.FromArgb(23, 105, 170);
            install.ForeColor = Color.White;
            install.FlatStyle = FlatStyle.Flat;
            install.FlatAppearance.BorderSize = 0;
            install.Click += async (s, e) => await InstallAsync();
            Controls.Add(install);

            remove.Text = "Remover Kontrol-ID";
            remove.Location = new Point(255, 325);
            remove.Size = new Size(185, 39);
            remove.FlatStyle = FlatStyle.Flat;
            remove.Click += async (s, e) => await RemoveAsync();
            Controls.Add(remove);

            log.Location = new Point(38, 387);
            log.Size = new Size(668, 204);
            log.Anchor = AnchorStyles.Top | AnchorStyles.Bottom | AnchorStyles.Left | AnchorStyles.Right;
            log.Multiline = true;
            log.ReadOnly = true;
            log.ScrollBars = ScrollBars.Vertical;
            log.BackColor = Color.White;
            log.Font = new Font("Consolas", 9F);
            Controls.Add(log);

            Shown += (s, e) => {
                if (!NativeMethods.IsAdministrator()) {
                    Append("Este aplicativo precisa ser executado como Administrador.");
                    install.Enabled = false;
                    remove.Enabled = false;
                } else {
                    Append("Pronto. Nenhum daemon será instalado nesta estação.");
                }
            };
        }

        private void AddField(string labelText, TextBox box, int top, string hint)
        {
            var label = new Label { Text = labelText, AutoSize = true, Location = new Point(38, top) };
            box.Location = new Point(38, top + 23);
            box.Size = new Size(668, 25);
            box.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;
            var note = new Label {
                Text = hint,
                AutoSize = true,
                ForeColor = Color.FromArgb(99, 113, 126),
                Location = new Point(40, top + 49)
            };
            Controls.Add(label);
            Controls.Add(box);
            Controls.Add(note);
        }

        private async Task InstallAsync()
        {
            Uri baseUri;
            string normalizedPin;
            if (!Uri.TryCreate(appliance.Text.Trim(), UriKind.Absolute, out baseUri) ||
                !String.Equals(baseUri.Scheme, Uri.UriSchemeHttps, StringComparison.OrdinalIgnoreCase)) {
                MessageBox.Show("Informe um endereço HTTPS válido para o Kontrol.", Text,
                    MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }
            normalizedPin = NormalizeThumbprint(fingerprint.Text);
            if (normalizedPin.Length != 64 || normalizedPin.Any(c => !Uri.IsHexDigit(c))) {
                MessageBox.Show("O SHA-256 deve conter 64 caracteres hexadecimais.", Text,
                    MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }
            if (String.IsNullOrWhiteSpace(token.Text)) {
                MessageBox.Show("Informe o token temporário de inscrição.", Text,
                    MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            SetBusy(true);
            log.Clear();
            try {
                string tokenValue = token.Text.Trim();
                await Task.Run(() => Provision(baseUri, tokenValue, normalizedPin));
                token.Clear();
                Append("Concluído. A estação está pronta para identificação automática.");
                MessageBox.Show("A configuração do Kontrol-ID foi concluída.", Text,
                    MessageBoxButtons.OK, MessageBoxIcon.Information);
            } catch (Exception ex) {
                Append("ERRO: " + ex.Message);
                MessageBox.Show(ex.Message, "Falha na configuração", MessageBoxButtons.OK,
                    MessageBoxIcon.Error);
            } finally {
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
                await Task.Run(() => RemoveProvisioning());
                Append("Configuração removida. Não há serviço Kontrol-ID residente.");
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
            install.Enabled = !busy;
            remove.Enabled = !busy;
            UseWaitCursor = busy;
        }

        private void Append(string message)
        {
            if (InvokeRequired) {
                BeginInvoke(new Action<string>(Append), message);
                return;
            }
            log.AppendText("[" + DateTime.Now.ToString("HH:mm:ss") + "] " + message + Environment.NewLine);
        }

        private void Provision(Uri baseUri, string enrollmentToken, string webGuiPin)
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

                Append("Enviando CSR ao Kontrol com validação por pin SHA-256...");
                var serializer = new JavaScriptSerializer { MaxJsonLength = 131072 };
                string requestBody = serializer.Serialize(new Dictionary<string, object> {
                    { "token", enrollmentToken }, { "hostname", host }, { "fqdn", fqdn }, { "csr", csr }
                });
                Uri endpoint = new Uri(baseUri.ToString().TrimEnd('/') + "/kontrol_id_enrollment.php");
                string responseBody = PostPinnedJson(endpoint, requestBody, webGuiPin);
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

        internal void ProvisionForAutomation(Uri baseUri, string enrollmentToken, string webGuiPin)
        {
            if (baseUri == null || !String.Equals(baseUri.Scheme, Uri.UriSchemeHttps, StringComparison.OrdinalIgnoreCase))
                throw new InvalidOperationException("O endereço do Kontrol deve usar HTTPS.");
            string pin = NormalizeThumbprint(webGuiPin);
            if (pin.Length != 64 || pin.Any(c => !Uri.IsHexDigit(c)))
                throw new InvalidOperationException("O pin SHA-256 da WebGUI é inválido.");
            if (String.IsNullOrWhiteSpace(enrollmentToken))
                throw new InvalidOperationException("O token de inscrição não foi informado.");
            Provision(baseUri, enrollmentToken, pin);
        }

        internal void RemoveForAutomation()
        {
            RemoveProvisioning();
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
                string script = BuildRemovalScript(certThumb, subject, resource, caThumb, account);
                RunProcess("powershell.exe", "-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command -",
                    script, true);
                RemoveCertificate(StoreName.My, certThumb);
                RemoveCertificate(StoreName.Root, caThumb);
                NativeMethods.DeleteLocalUser(account);
            }
            Registry.LocalMachine.DeleteSubKeyTree(@"SOFTWARE\Kontrol\Kontrol-ID", false);
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
