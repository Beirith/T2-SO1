#include "fs.h"
#include <vector>

// Ok
int INE5412_FS::fs_format()
{
	union fs_block block;
	fs_inode *inode;
	int ninodeblocks;

	disk->read(0, block.data);
	ninodeblocks = block.super.ninodeblocks;

	// Verifica se o sistema de arquivos está montado
	if (mounted)
	{
		cout << "ERROR: filesystem is already mounted!\n";
		return 0;
	}

	// Seta e escreve os valores do superbloco
	block.super.magic = FS_MAGIC;
	block.super.nblocks = disk->size();
	block.super.ninodeblocks = (disk->size() + 9) / 10;
	block.super.ninodes = block.super.ninodeblocks * INODES_PER_BLOCK;

	disk->write(0, block.data);

	// Formata todos os inodes de todos os blocos de inode
    for (int i = 1; i <= ninodeblocks; ++i) {
        for (int j = 0; j < INODES_PER_BLOCK; ++j) {
			inode = &block.inode[j];
			inode_format(inode);
        }

        disk->write(i, block.data);
    }

    return 1;                
}

// Ok
void INE5412_FS::fs_debug()
{
	union fs_block block;
	fs_inode inode;
	int direct;
	int indirect;
	int pointers;

	// Inicialmente, lê o Superbloco (bloco 0) e exibe suas respectivas informações
	disk->read(0, block.data);
	int ninodeblocks = block.super.ninodeblocks;

	cout << "superblock:\n";
	cout << "    " << (block.super.magic == FS_MAGIC ? "magic number is valid\n" : "magic number is invalid!\n");
 	cout << "    " << block.super.nblocks << " blocks\n";
	cout << "    " << block.super.ninodeblocks << " inode blocks\n";
	cout << "    " << block.super.ninodes << " inodes\n";

	// Em seguida, lê os blocos de Inode
	for (int i = 0; i < ninodeblocks; i++) 
	{
		for (int j = 0; j < INODES_PER_BLOCK; j++)
		{
			// O bloco é sempre lido novamente a cada Inode percorrido,
			// por conta da possibilidade de leitura de um bloco indireto,
			// que irá subscrever os dados do bloco de inode em block.data
			disk->read(i + 1, block.data);
			inode = block.inode[j];

			if (inode.isvalid)
			{
				cout << "inode " << i * INODES_PER_BLOCK + j << ":\n";
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
					disk->read(indirect, block.data);
					for (int k = 0; k < POINTERS_PER_BLOCK; k++)
					{
						pointers = block.pointers[k];
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
	union fs_block block;
	fs_inode inode;
	int ninodeblocks;
	int direct;
	int indirect;
	int pointers;
	int nblocks;

	disk->read(0, block.data);
	ninodeblocks = block.super.ninodeblocks;
	nblocks = block.super.nblocks;

	bitmap.free_blocks = std::vector<bool>(block.super.nblocks, false);
	bitmap.free_blocks[0] = true;

	for (int i = 0; i < ninodeblocks; i++) 
	{
		bitmap.free_blocks[i + 1] = true;
		for (int j = 0; j < INODES_PER_BLOCK; j++)
		{
			disk->read(i + 1, block.data);
			inode = block.inode[j];

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

					disk->read(indirect, block.data);
					for (int k = 0; k < POINTERS_PER_BLOCK; k++)
					{
						pointers = block.pointers[k];
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
	print_bitmap(nblocks);

	return 1;
}

// Ok
int INE5412_FS::fs_create()
{
	union fs_block block;
	fs_inode inode;
	int ninodeblocks;

	disk->read(0, block.data);
	ninodeblocks = block.super.ninodeblocks;

	for (int i = 0; i < ninodeblocks; i++)
	{
		disk->read(i + 1, block.data);
		cout << "inode block: " << i + 1 << "\n";
		for (int j = 0; j < INODES_PER_BLOCK; j++) 
		{
			inode = block.inode[j];
			if (!inode.isvalid)
			{
				inode.isvalid = 1;
				inode.size = 0;
				inode.indirect = 0;
				for (int k = 0; k < POINTERS_PER_INODE; k++)
				{
					inode.direct[k] = 0;
				}

				block.inode[j] = inode;
				disk->write(i + 1, block.data);
				return (i * INODES_PER_BLOCK + j) + 1;
			}
		}
	}

	cout << "ERROR: no free inodes available!\n";
	return 0;
}

int INE5412_FS::fs_delete(int inumber)
{
	print_bitmap(20);
	return 0;
}

int INE5412_FS::fs_getsize(int inumber)
{
	return -1;
}

int INE5412_FS::fs_read(int inumber, char *data, int length, int offset)
{
	return 0;
}

int INE5412_FS::fs_write(int inumber, const char *data, int length, int offset)
{
	return 0;
}

// Funções auxiliares
void INE5412_FS::inode_load(int inumber, class fs_inode *inode)
{
	// for (int i = 0; i < INODES_PER_BLOCK; i++)
	// {
	// 	if (i == inumber % INODES_PER_BLOCK)
	// 	{
	// 		disk->read((inumber / INODES_PER_BLOCK) + 1, inode);
	// 		return;
	// 	}
	// }
}

void INE5412_FS::inode_save(int inumber, class fs_inode *inode)
{
	// union fs_block block;
	// disk->read((inumber / INODES_PER_BLOCK) + 1, block.data);
	// block.inode[inumber % INODES_PER_BLOCK] = *inode;
	// disk->write((inumber / INODES_PER_BLOCK) + 1, block.data);
}

void INE5412_FS::inode_format(class fs_inode *inode)
{
	inode->isvalid = 0;
	inode->size = 0;
	inode->indirect = 0;
	for (int k = 0; k < POINTERS_PER_INODE; ++k) 
	{
		inode->direct[k] = 0;
	}
}

// Fins de teste
void INE5412_FS::print_bitmap(int nblocks)
{
	cout << "bitmap: ";
	for (int i = 0; i < nblocks; i++)
	{
		cout << bitmap.free_blocks[i] << " ";
	}
	cout << "\n";
}
