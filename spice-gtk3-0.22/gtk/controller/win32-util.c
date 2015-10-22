/*
   Copyright (C) 2012 Red Hat, Inc.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#include "win32-util.h"
#include <windows.h>
#include <sddl.h>
#include <aclapi.h>

gboolean
spice_win32_set_low_integrity (void* handle, GError **error)
{
    g_return_val_if_fail (handle != NULL, FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    /* see also http://msdn.microsoft.com/en-us/library/bb625960.aspx */
    PSECURITY_DESCRIPTOR psd = NULL;
    PACL psacl = NULL;
    BOOL sacl_present = FALSE;
    BOOL sacl_defaulted = FALSE;
    char *emsg;
    int errsv;
    gboolean success = FALSE;

    if (!ConvertStringSecurityDescriptorToSecurityDescriptor ("S:(ML;;NW;;;LW)",
                                                              SDDL_REVISION_1, &psd, NULL))
        goto failed;

    if (!GetSecurityDescriptorSacl (psd, &sacl_present, &psacl, &sacl_defaulted))
        goto failed;

    if (SetSecurityInfo (handle, SE_KERNEL_OBJECT, LABEL_SECURITY_INFORMATION,
                         NULL, NULL, NULL, psacl) != ERROR_SUCCESS)
        goto failed;

    success = TRUE;
    goto end;

failed:
    errsv = GetLastError ();
    emsg = g_win32_error_message (errsv);
    g_set_error (error, G_IO_ERROR,
                 g_io_error_from_win32_error (errsv),
                 "Error setting integrity: %s",
                 emsg);
    g_free (emsg);

end:
    if (psd != NULL)
        LocalFree (psd);

    return success;
}

static gboolean
get_user_security_attributes (SECURITY_ATTRIBUTES* psa, SECURITY_DESCRIPTOR* psd, PACL* ppdacl)
{
    EXPLICIT_ACCESS ea;
    TRUSTEE trst;
    DWORD ret = 0;

    ZeroMemory (psa, sizeof (*psa));
    ZeroMemory (psd, sizeof (*psd));
    psa->nLength = sizeof (*psa);
    psa->bInheritHandle = FALSE;
    psa->lpSecurityDescriptor = psd;

    ZeroMemory (&trst, sizeof (trst));
    trst.pMultipleTrustee = NULL;
    trst.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
    trst.TrusteeForm = TRUSTEE_IS_NAME;
    trst.TrusteeType = TRUSTEE_IS_USER;
    trst.ptstrName = "CURRENT_USER";

    ZeroMemory (&ea, sizeof (ea));
    ea.grfAccessPermissions = GENERIC_WRITE | GENERIC_READ;
    ea.grfAccessMode = SET_ACCESS;
    ea.grfInheritance = NO_INHERITANCE;
    ea.Trustee = trst;

    ret = SetEntriesInAcl (1, &ea, NULL, ppdacl);
    if (ret != ERROR_SUCCESS)
        return FALSE;

   if (!InitializeSecurityDescriptor (psd, SECURITY_DESCRIPTOR_REVISION))
       return FALSE;

   if (!SetSecurityDescriptorDacl (psd, TRUE, *ppdacl, FALSE))
       return FALSE;

   return TRUE;
}

#define DEFAULT_PIPE_BUF_SIZE 4096

SpiceNamedPipe*
spice_win32_user_pipe_new (gchar *name, GError **error)
{
    SECURITY_ATTRIBUTES sa;
    SECURITY_DESCRIPTOR sd;
    PACL dacl = NULL;
    HANDLE pipe;
    SpiceNamedPipe *np = NULL;

    g_return_val_if_fail (name != NULL, NULL);
    g_return_val_if_fail (error != NULL, NULL);

    if (!get_user_security_attributes (&sa, &sd, &dacl))
        return NULL;

    pipe = CreateNamedPipe (name,
        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED |
    /* FIXME: why is FILE_FLAG_FIRST_PIPE_INSTANCE needed for WRITE_DAC
     * (apparently needed by SetSecurityInfo). This will prevent
     * multiple pipe listener....?! */
        FILE_FLAG_FIRST_PIPE_INSTANCE | WRITE_DAC,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        PIPE_UNLIMITED_INSTANCES,
        DEFAULT_PIPE_BUF_SIZE, DEFAULT_PIPE_BUF_SIZE,
        0, &sa);

    if (pipe == INVALID_HANDLE_VALUE) {
        int errsv = GetLastError ();
        gchar *emsg = g_win32_error_message (errsv);

        g_set_error (error,
                     G_IO_ERROR,
                     g_io_error_from_win32_error (errsv),
                     "Error CreateNamedPipe(): %s",
                     emsg);

        g_free (emsg);
        goto end;
    }

    /* lower integrity on Vista/Win7+ */
    if ((LOBYTE (g_win32_get_windows_version()) > 0x05) &&
        !spice_win32_set_low_integrity (pipe, error))
        goto end;

    np = SPICE_NAMED_PIPE (g_initable_new (SPICE_TYPE_NAMED_PIPE,
                                           NULL, error, "handle", pipe, NULL));

end:
    LocalFree (dacl);

    return np;
}
