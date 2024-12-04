#include "fs.h"
#include <vector>
#include <cstring> 

// Ok
int INE5412_FS::fs_format()
{
	disk->read(0, current_block.data);
	ninodeblocks = current_block.super.ninodeblocks;

	// Verifica se o sistema de arquivos está montado
	if (mounted)
	{
		cout << "ERROR: filesystem is already mounted!\n";
		return 0;
	}

	// Seta e escreve os valores do superbloco
	current_block.super.magic = FS_MAGIC;
	current_block.super.nblocks = disk->size();
	current_block.super.ninodeblocks = (disk->size() + 9) / 10;
	current_block.super.ninodes = ninodeblocks * INODES_PER_BLOCK;

	fs_inode *inode;
	disk->write(0, current_block.data);

	// Formata todos os inodes de todos os blocos de inode
    for (int i = 1; i <= ninodeblocks; ++i) {
        for (int j = 0; j < INODES_PER_BLOCK; ++j) {
			inode = &current_block.inode[j];
			inode_format(inode);
        }

        disk->write(i, current_block.data);
    }

    return 1;                
}

// Ok
void INE5412_FS::fs_debug()
{
	fs_inode inode;
	int direct;
	int indirect;
	int pointers;

	// Inicialmente, lê o Superbloco (bloco 0) e exibe suas respectivas informações
	disk->read(0, current_block.data);
	ninodeblocks = current_block.super.ninodeblocks;

	cout << "superblock:\n";
	cout << "    " << (current_block.super.magic == FS_MAGIC ? "magic number is valid\n" : "magic number is invalid!\n");
 	cout << "    " << current_block.super.nblocks << " blocks\n";
	cout << "    " << current_block.super.ninodeblocks << " inode blocks\n";
	cout << "    " << current_block.super.ninodes << " inodes\n";

	// Em seguida, lê os blocos de Inode
	for (int i = 0; i < ninodeblocks; i++) 
	{
		for (int j = 0; j < INODES_PER_BLOCK; j++)
		{
			// O bloco é sempre lido novamente a cada Inode percorrido,
			// por conta da possibilidade de leitura de um bloco indireto,
			// que irá subscrever os dados do bloco de inode em block.data
			disk->read(i + 1, current_block.data);
			inode = current_block.inode[j];

			if (inode.isvalid)
			{
				cout << "inode " << i * INODES_PER_BLOCK + j + 1 << ":\n";
				cout << "    size: " << inode.size << " bytes\n";

				cout << "    direct blocks:";
				for (int k = 0; k < POINTERS_PER_INODE; k++)
				{
					direct = inode.direct[k];
					if (direct != 0)
					{
						cout << " " << direct;
					}
				}
				cout << "\n";

				indirect = inode.indirect;
				if (indirect != 0)
				{
					cout << "    indirect block: " << indirect << "\n";
					cout << "    indirect data blocks:";

					// Lê o bloco indireto e exibe seus ponteiros
					disk->read(indirect, current_block.data);
					for (int k = 0; k < POINTERS_PER_BLOCK; k++)
					{
						pointers = current_block.pointers[k];
						if (pointers != 0)
						{
							cout << " " << pointers;
						}
					}
					cout << "\n";
				}
			}
		}
	}
}

// Ok
int INE5412_FS::fs_mount()
{
	fs_inode inode;
	int direct;
	int indirect;
	int pointers;

	disk->read(0, current_block.data);
	ninodeblocks = current_block.super.ninodeblocks;
	max_inumber = ninodeblocks * INODES_PER_BLOCK;
	nblocks = current_block.super.nblocks;

	bitmap.free_blocks = std::vector<bool>(current_block.super.nblocks, false);
	bitmap.free_blocks[0] = true;

	for (int i = 0; i < ninodeblocks; i++) 
	{
		bitmap.free_blocks[i + 1] = true;
		for (int j = 0; j < INODES_PER_BLOCK; j++)
		{
			disk->read(i + 1, current_block.data);
			inode = current_block.inode[j];

			if (inode.isvalid)
			{
				for (int k = 0; k < POINTERS_PER_INODE; k++)
				{
					direct = inode.direct[k];
					if (direct != 0)
					{
						bitmap.free_blocks[direct] = true;
					}
				}

				indirect = inode.indirect;
				if (indirect != 0)
				{
					bitmap.free_blocks[indirect] = true;

					disk->read(indirect, current_block.data);
					for (int k = 0; k < POINTERS_PER_BLOCK; k++)
					{
						pointers = current_block.pointers[k];
						if (pointers != 0)
						{
							bitmap.free_blocks[pointers] = true;
						}
					}
				}
			}
		}
	}
	mounted = true;
	// print_bitmap();

	return 1;
}

// Ok
int INE5412_FS::fs_create()
{
	if (!is_mounted())
	{
		return 0;
	}

	fs_inode inode;

	disk->read(0, current_block.data);

	for (int i = 0; i < ninodeblocks; i++)
	{
		disk->read(i + 1, current_block.data);

		for (int j = 0; j < INODES_PER_BLOCK; j++) 
		{
			inode = current_block.inode[j];
			if (!inode.isvalid)
			{
				inode.isvalid = 1;
				inode.size = 0;
				inode.indirect = 0;
				for (int k = 0; k < POINTERS_PER_INODE; k++)
				{
					inode.direct[k] = 0;
				}

				current_block.inode[j] = inode;
				disk->write(i + 1, current_block.data);
				return (i * INODES_PER_BLOCK + j) + 1;
			}
		}
	}

	cout << "ERROR: no free inodes available!\n";
	return 0;
}

// Ok
int INE5412_FS::fs_delete(int inumber)
{
	if (!is_mounted())
	{
		return 0;
	}

	disk->read(0, current_block.data);
	int adjusted_inumber = inumber - 1;

	if (inumber < 1 or inumber > max_inumber)
	{
		cout << "ERROR: invalid inode number!\n";
		cout << "inode number must be between 1 and " << max_inumber << "\n";
		return 0;
	}

	fs_inode inode;
	inode_load(adjusted_inumber, &inode);

	if (!inode.isvalid)
	{
		cout << "ERROR: inode is not valid!\n";
		return 0;
	}

	inode_format(&inode);
	inode_save(adjusted_inumber, &inode);

    return 1;
}

// Ok
int INE5412_FS::fs_getsize(int inumber)
{
	fs_inode inode;
	int adjusted_inumber = inumber - 1;

	inode_load(adjusted_inumber, &inode);

	if (inode.isvalid)
	{
		return inode.size;
	}
	
	cout << "ERROR: inode is not valid!\n";
	return -1;
}

// Ok
int INE5412_FS::fs_read(int inumber, char *data, int length, int offset)
{
	fs_inode inode;
	int adjusted_inumber = inumber - 1;
	inode_load(adjusted_inumber, &inode);

	if (validate(&inode, inumber, offset) < 0)
	{
		return 0;
	}

	union fs_block block;
	int block_number = (adjusted_inumber / INODES_PER_BLOCK) + 1;

	disk->read(block_number, current_block.data);

	int nbytes_read = 0;
	int block_size = Disk::DISK_BLOCK_SIZE;
	int starting_block = offset / block_size;
	int block_offset = offset % block_size;

	if (offset + length > inode.size)
	{
		length = inode.size - offset;
	}

	for (int i = starting_block; i < POINTERS_PER_INODE; i++)
	{
		int direct = inode.direct[i];
		if (direct == 0)
		{
			continue;
		}

		if (nbytes_read >= length)
		{
			break;
		}

		disk->read(direct, block.data);

		int portion_size = std::min(block_size - block_offset, length - nbytes_read);
		memcpy(data + nbytes_read, block.data + block_offset, portion_size);

		nbytes_read += portion_size;
		block_offset = 0;
	}

	if (nbytes_read < length && inode.indirect != 0)
	{
		union fs_block indirect_block;
		disk->read(inode.indirect, indirect_block.data);

		for (int i = 0; i < POINTERS_PER_BLOCK; ++i)
		{
			int inode_pointers = indirect_block.pointers[i];
			if (inode_pointers == 0)
			{
				continue;
			}

			if (nbytes_read >= length)
			{
				break;
			}

			disk->read(inode_pointers, block.data);

			int portion_size = std::min(block_size - block_offset, length - nbytes_read);
			memcpy(data + nbytes_read, block.data + block_offset, portion_size);

			nbytes_read += portion_size;
			block_offset = 0;
		}
	}

	return nbytes_read;
}

// Ok
int INE5412_FS::fs_write(int inumber, const char *data, int length, int offset)
{
    fs_inode inode;
    int adjusted_inumber = inumber - 1;
    inode_load(adjusted_inumber, &inode);

    if (validate(&inode, inumber, offset) < 0) 
	{
        return 0;
    }

    int nbytes_written = 0;
    int block_size = Disk::DISK_BLOCK_SIZE;
    int block_offset = offset % block_size;
    int block_index = offset / block_size;
    int inode_size = inode.size;

    while (nbytes_written < length) 
	{
        int block_number = 0;

        if (block_index < POINTERS_PER_INODE) {

            if (inode.direct[block_index] == 0) {

                int new_block = get_free_block();
                if (new_block == -1) 
				{
                    std::cerr << "ERROR: Disk is full (cannot allocate direct block).\n";
                    break;
                }

                inode.direct[block_index] = new_block;
            }

            block_number = inode.direct[block_index];
        } 
        else
		{
            int indirect_index = block_index - POINTERS_PER_INODE;

            if (inode.indirect == 0) {

                int new_block = get_free_block();
                if (new_block == -1) {
                    std::cerr << "ERROR: Disk is full (cannot allocate indirect block).\n";
                    break;
                }

                inode.indirect = new_block;

                union fs_block new_indirect_block = {};
                disk->write(inode.indirect, new_indirect_block.data);
            }

            union fs_block indirect_block;
            disk->read(inode.indirect, indirect_block.data);

            if (indirect_block.pointers[indirect_index] == 0) 
			{

                int new_block = get_free_block();
                if (new_block == -1) {
                    std::cerr << "ERROR: Disk is full (cannot allocate indirect block).\n";
                    break;
                }

                indirect_block.pointers[indirect_index] = new_block;
                disk->write(inode.indirect, indirect_block.data);
            }

            block_number = indirect_block.pointers[indirect_index];
        }

        union fs_block block;
        disk->read(block_number, block.data);

        int portion_size = std::min(length - nbytes_written, block_size - block_offset);
        memcpy(block.data + block_offset, data + nbytes_written, portion_size);

        disk->write(block_number, block.data);

        nbytes_written += portion_size;
        block_offset = 0;
        block_index++;
    }

    inode.size = std::max(inode_size, offset + nbytes_written);

    inode_save(adjusted_inumber, &inode);

    return nbytes_written;
}

// Funções auxiliares
void INE5412_FS::inode_load(int inumber, class fs_inode *inode)
{
	int block_number = (inumber / INODES_PER_BLOCK) + 1;
	int inode_number = inumber % INODES_PER_BLOCK;

	disk->read(block_number, current_block.data);

	*inode = current_block.inode[inode_number];
}

void INE5412_FS::inode_save(int inumber, class fs_inode *inode)
{
    int block_number = (inumber / INODES_PER_BLOCK) + 1;
    int inode_number = inumber % INODES_PER_BLOCK;

    disk->read(block_number, current_block.data);

    current_block.inode[inode_number] = *inode;

    disk->write(block_number, current_block.data);
}

void INE5412_FS::inode_format(class fs_inode *inode)
{
	inode->isvalid = 0;
	inode->size = 0;
	inode->indirect = 0;

	if (mounted)
	{
		bitmap.free_blocks[inode->indirect] = false;
	}

	for (int k = 0; k < POINTERS_PER_INODE; k++) 
	{
		inode->direct[k] = 0;
		if (mounted)
		{
			bitmap.free_blocks[inode->direct[k]] = false;
		}
	}
}

int INE5412_FS::is_mounted()
{
	if (mounted)
	{
		return 1;
	}
	else
	{
		cout << "ERROR: filesystem is not mounted!\n";
		return 0;
	}
}

int INE5412_FS::validate(fs_inode *inode, int inumber, int offset)
{
	if (!is_mounted())
	{
		return -1;
	}

	if (inumber < 1 or inumber > max_inumber)
	{
		cout << "ERROR: invalid inode number!\n";
		cout << "inode number must be between 1 and " << max_inumber << "\n";
		return -1;
	}

	if (!inode->isvalid)
	{
		cout << "ERROR: inode is not valid!\n";
		return -1;
	}

	// if (offset >= inode->size)
	// {
	// 	cout << "ERROR: offset is greater than inode size!\n";
	// 	return -1;
	// }

	return 1;
}

int INE5412_FS::get_free_block()
{
	for (int i = 0; i < nblocks; i++)
	{
		if (bitmap.free_blocks[i] == false)
		{
			bitmap.free_blocks[i] = true;
			return i;
		}
	}

	return -1;
}

// Funções de teste
void INE5412_FS::print_bitmap()
{
	cout << "bitmap: ";
	for (int i = 0; i < nblocks; i++)
	{
		cout << bitmap.free_blocks[i] << " ";
	}
	cout << "\n";
}