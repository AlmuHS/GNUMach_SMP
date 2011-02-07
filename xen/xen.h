/*
 *  Copyright (C) 2006-2010, 2011 Samuel Thibault <samuel.thibault@ens-lyon.org>
 *
 * This program is free software ; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation ; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY ; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the program ; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef XEN_XEN_H
#define XEN_XEN_H

void hyp_init(void);
void hyp_dev_init(void);
void hyp_invalidate_pte(pt_entry_t *pte);
void hyp_idle(void);
void hyp_p2m_init(void);

#endif /* XEN_XEN_H */
